main: main.c
	gcc -o lab0 -g main.c
check: main
	@echo "Test1: file input and output"
	@rm -rf outputfile.txt
	@echo "good\ntest" > inputfile.txt
	@./lab0 --input=inputfile.txt --output=outputfile.txt
	@diff inputfile.txt outputfile.txt > /dev/null || (echo "Error: input and output file content mismatch: diff returns $$?, content mismatch")
	@rm -rf inputfile.txt
	@rm -rf outputfile.txt

	@echo "Test2: sigsegv and capturing"
	@./lab0 --segfault --catch > /dev/null 2>&1 || [ $$? -eq 3 ] || (echo "Error: got unexpected output from sigsegv capturing!")

	@echo "Tests done!"
clean:
	rm -rf lab0
	rm -rf *.o
dist:
	rm -rf lab0-404380075.tar.gz
	tar -cf lab0-404380075.tar.gz *.c Makefile *.png README