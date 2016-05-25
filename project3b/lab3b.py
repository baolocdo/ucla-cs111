import sys
import os

import csv

class Inode(object):
  def __init__(self, number, linksCount):
    self._number = number
    self._referencedBy = []
    self._linksCount = linksCount
    self._ptrList = []

    return

class Block(object):
  def __init__(self, number):
    self._number = number
    self._referencedBy = []
    return

class CsvParser(object):
  def __init__(self):
    self._blocksCnt = 0
    self._inodesCnt = 0
    self._blockSize = 0
    self._blocksPerGroup = 0
    self._inodesPerGroup = 0

    self._inodeDict = dict()
    self._blockDict = dict()
    self._indirectDict = dict()
    self._directoryDict = dict()

    self._inodeBitmapBlocks = []
    self._blockBitmapBlocks = []

    self._inodeFreeList = []
    self._blockFreeList = []

    self._unallocatedInodes = dict()
    self._incorrectEntries = []
    self._invalidBlocks = []

    return

  def readSuperblock(self, fileName = "super.csv"):
    with open(fileName, 'r') as csvFile:
      reader = csv.reader(csvFile, delimiter=',', quotechar='|')
      for row in reader:
        self._inodesCnt = int(row[1])
        self._blocksCnt = int(row[2])
        self._blockSize = int(row[3])
        self._blocksPerGroup = int(row[5])
        self._inodesPerGroup = int(row[6])
    return

  def readGroup(self, fileName = "group.csv"):
    with open(fileName, 'r') as csvFile:
      reader = csv.reader(csvFile, delimiter=',', quotechar='|')
      for row in reader:
        self._inodeBitmapBlocks.append(int(row[4], 16))
        self._blockBitmapBlocks.append(int(row[5], 16))

        inodeBitmapBlock = Block(int(row[4], 16))
        self._blockDict[int(row[4], 16)] = inodeBitmapBlock
        blockBitmapBlock = Block(int(row[5], 16))
        self._blockDict[int(row[5], 16)] = blockBitmapBlock
    return

  def readBitmap(self, fileName = "bitmap.csv"):
    with open(fileName, 'r') as csvFile:
      reader = csv.reader(csvFile, delimiter=',', quotechar='|')
      for row in reader:
        bitmapBlockNumber = int(row[0], 16)
        if bitmapBlockNumber in self._inodeBitmapBlocks:
          self._inodeFreeList.append(int(row[1]))
        elif bitmapBlockNumber in self._blockBitmapBlocks:
          self._blockFreeList.append(int(row[1]))
        # TODO: check if we should output an error here if the source block's neither an inode bitmap nor block bitmap
    return

  def readIndirect(self, fileName = "indirect.csv"):
    with open(fileName, 'r') as csvFile:
      reader = csv.reader(csvFile, delimiter=',', quotechar='|')
      for row in reader:
        containingBlock = int(row[0], 16)
        entryNum = int(row[1])
        pointerValue = int(row[2], 16)
        if containingBlock in self._indirectDict:
          self._indirectDict[containingBlock].append((entryNum, pointerValue))
        else:
          self._indirectDict[containingBlock] = [(entryNum, pointerValue)]
    return

  def readDirectory(self, fileName = "directory.csv"):
    with open(fileName, 'r') as csvFile:
      reader = csv.reader(csvFile, delimiter=',', quotechar='\"')
      for row in reader:
        childInodeNum = int(row[4])
        parentInodeNum = int(row[0])
        entryNum = int(row[1])
        entryName = row[5]
        if childInodeNum != parentInodeNum or parentInodeNum == 2:
          self._directoryDict[childInodeNum] = parentInodeNum
        if childInodeNum in self._inodeDict:
          self._inodeDict[childInodeNum]._referencedBy.append((parentInodeNum, entryNum))
        else:
          if childInodeNum in self._unallocatedInodes:
            self._unallocatedInodes[childInodeNum].append((parentInodeNum, entryNum))
          else:
            self._unallocatedInodes[childInodeNum] = [(parentInodeNum, entryNum)]
        if entryNum == 0:
          if childInodeNum != parentInodeNum:
            self._incorrectEntries.append((parentInodeNum, entryName, childInodeNum, parentInodeNum))
        elif entryNum == 1:
          if parentInodeNum not in self._directoryDict or childInodeNum != self._directoryDict[parentInodeNum]:
            self._incorrectEntries.append((parentInodeNum, entryName, childInodeNum, self._directoryDict[parentInodeNum]))

    return

  def readInode(self, fileName = "inode.csv"):
    with open(fileName, 'r') as csvFile:
      reader = csv.reader(csvFile, delimiter=',', quotechar='\"')
      for row in reader:
        numberOfBlocks = int(row[10])
        inodeNumber = int(row[0])
        linksCount = int(row[5])
        self._inodeDict[inodeNumber] = Inode(inodeNumber, linksCount)

        # direct data blocks handling
        upperBound = min(12, numberOfBlocks) + 11
        for i in range(11, upperBound):
          blockNumber = int(row[i], 16)
          self.incrementBlockReferencedBy(blockNumber, inodeNumber, 0, i - 11)
        
        # indirect data blocks handling
        remainingBlocks = numberOfBlocks - 12
        if remainingBlocks > 0:
          # singly indirect blocks
          blockNumber = int(row[23], 16)
          if blockNumber == 0 or blockNumber > self._blocksCnt:
            self._invalidBlocks.append((blockNumber, inodeNumber, 0, 12))
          else:
            cnt = self.scanIndirectBlock(blockNumber, inodeNumber, 0, 12)
            remainingBlocks -= cnt

        if remainingBlocks > 0:
          # doubly indirect blocks
          blockNumber = int(row[24], 16)
          if blockNumber == 0 or blockNumber > self._blocksCnt:
            self._invalidBlocks.append((blockNumber, inodeNumber, 0, 13))
          else:
            cnt = self.scanDoublyIndirectBlock(blockNumber, inodeNumber, 0, 13)
            remainingBlocks -= cnt

        if remainingBlocks > 0:
          # triply indirect blocks
          blockNumber = int(row[25], 16)
          if blockNumber == 0 or blockNumber > self._blocksCnt:
            self._invalidBlocks.append((blockNumber, inodeNumber, 0, 14))
          else:
            cnt = self.scanTriplyIndirectBlock(blockNumber, inodeNumber, 0, 14)
            remainingBlocks -= cnt
    return

  # scans the indirect block as given by blockNumber, increments the referencedBy for the data blocks as well as the indirect block for the inode;
  # returns the total the number of data blocks visited + 1 for the indirect block itself
  def scanIndirectBlock(self, blockNumber, inodeNumber, indirectBlockNumber, entryNum):
    cnt = 1
    self.incrementBlockReferencedBy(blockNumber, inodeNumber, indirectBlockNumber, entryNum)
    if blockNumber in self._indirectDict:
      for entry in self._indirectDict[blockNumber]:
        self.incrementBlockReferencedBy(entry[1], inodeNumber, blockNumber, entry[0])
        cnt += 1
    return cnt

  def scanDoublyIndirectBlock(self, blockNumber, inodeNumber, indirectBlockNumber, entryNum):
    cnt = 1
    self.incrementBlockReferencedBy(blockNumber, inodeNumber, indirectBlockNumber, entryNum)
    if blockNumber in self._indirectDict:
      for entry in self._indirectDict[blockNumber]:
        cnt += self.scanIndirectBlock(entry[1], inodeNumber, blockNumber, entry[0])
    return cnt

  def scanTriplyIndirectBlock(self, blockNumber, inodeNumber, indirectBlockNumber, entryNum):
    cnt = 1
    self.incrementBlockReferencedBy(blockNumber, inodeNumber, indirectBlockNumber, entryNum)
    if blockNumber in self._indirectDict:
      for entry in self._indirectDict[blockNumber]:
        cnt += self.scanDoublyIndirectBlock(entry[1], inodeNumber, blockNumber, entry[0])
    return cnt

  def incrementBlockReferencedBy(self, blockNumber, inodeNumber, indirectBlockNumber, entryNum):
    if blockNumber == 0 or blockNumber > self._blocksCnt:
      self._invalidBlocks.append((blockNumber, inodeNumber, indirectBlockNumber, entryNum))
    else:
      # check if we need a dict to store the referenced by; check if this null handling is correct
      if blockNumber not in self._blockDict:
        self._blockDict[blockNumber] = Block(blockNumber)
      self._blockDict[blockNumber]._referencedBy.append((inodeNumber, indirectBlockNumber, entryNum))
    return

  def readFiles(self):
    self.readSuperblock()
    self.readGroup()
    self.readBitmap()
    self.readIndirect()
    self.readInode()
    self.readDirectory()
    return

  def writeErrors(self, filename = "my_check.txt"):
    outputFile = open(filename, 'w')

    for entry in sorted(self._unallocatedInodes):
      writeStr = "UNALLOCATED INODE < " + str(entry) + " > REFERENCED BY "
      for item in sorted(self._unallocatedInodes[entry]):
        writeStr += "DIRECTORY < " + str(item[0]) + " > ENTRY < " + str(item[1]) + " > "
        outputFile.write(writeStr.strip() + "\n")

    for entry in sorted(self._incorrectEntries):
      outputFile.write("INCORRECT ENTRY IN < " + str(entry[0]) + " > NAME < " + str(entry[1]) + " > LINK TO < " + str(entry[2]) + " > SHOULD BE < " + str(entry[3]) + " >\n")

    for entry in sorted(self._inodeDict):
      linkCount = len(self._inodeDict[entry]._referencedBy)

      if entry > 10 and linkCount == 0:
        bitmapBlockNum = self._inodeBitmapBlocks[entry / self._inodesPerGroup]
        outputFile.write("MISSING INODE < " + str(entry) + " > SHOULD BE IN FREE LIST < " + str(bitmapBlockNum) + " >\n")
      elif linkCount != self._inodeDict[entry]._linksCount:
        outputFile.write("LINKCOUNT < " + str(entry) + " > IS < " + str(self._inodeDict[entry]._linksCount) + " > SHOULD BE < " + str(linkCount) + " >\n")

      # If inode is in both free list and allocated list, we think of it as unallocated
      #   previous check at directory only finds those that are mentioned by directory but not in allocated list
      # This is inferred from Zhaoxing's session, and is not tested by the test case
      if entry in self._inodeFreeList:
        outputFile.write("UNALLOCATED INODE < " + str(entry) + " >\n")

    # If inode is in neither free list nor allocated list, we think of it as missing
    # This is inferred from Zhaoxing's session, and is not tested by the test case
    for i in range(11, self._inodesCnt):
      if i not in self._inodeFreeList and i not in self._inodeDict:
        bitmapBlockNum = self._inodeBitmapBlocks[i / self._inodesPerGroup]
        outputFile.write("MISSING INODE < " + str(i) + " > SHOULD BE IN FREE LIST < " + str(bitmapBlockNum) + " >\n")

    for entry in sorted(self._blockDict):
      if len(self._blockDict[entry]._referencedBy) > 1:
        writeStr = "MULTIPLY REFERENCED BLOCK < " + str(entry) + " > BY "
        for item in sorted(self._blockDict[entry]._referencedBy):
          if item[1] == 0:
            writeStr += "INODE < " + str(item[0]) + " > ENTRY < " + str(item[2]) + " > "
          else:
            writeStr += "INODE < " + str(item[0]) + " > INDIRECT BLOCK < " + str(item[1]) + " > ENTRY < " + str(item[2]) + " > "
        outputFile.write(writeStr.strip() + "\n")
      if entry in self._blockFreeList:
        writeStr = "UNALLOCATED BLOCK < " + str(entry) + " > REFERENCED BY "
        for item in sorted(self._blockDict[entry]._referencedBy):
          if item[1] == 0:
            writeStr += "INODE < " + str(item[0]) + " > ENTRY < " + str(item[2]) + " > "
          else:
            writeStr += "INODE < " + str(item[0]) + " > INDIRECT BLOCK < " + str(item[1]) + " > ENTRY < " + str(item[2]) + " > "
        outputFile.write(writeStr.strip() + "\n")      

    # invalid block entry
    for entry in sorted(self._invalidBlocks):
      outputFile.write("INVALID BLOCK < " + str(entry[0]) + " > IN INODE < " + str(entry[1]) + " > (INDIRECT BLOCK < " + str(entry[2]) + " >) ENTRY < " + str(entry[3]) + " >\n")
    
    outputFile.close()
    return

if __name__ == "__main__":
  csvParser = CsvParser()
  csvParser.readFiles()
  csvParser.writeErrors()
