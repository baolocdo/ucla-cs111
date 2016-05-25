main: lab3b.py
	python lab3b.py
run:
	python lab3b.py
dist:
	rm -rf lab3a-404380075.tar.gz
	tar -zcf lab3a-404380075.tar.gz *.py Makefile README
check:
	python lab3b.py
	sort lab3b_check_given.txt > lab3b_check_given_sorted.txt
	sort lab3b_check.txt > lab3b_check_sorted.txt
	diff lab3b_check_given_sorted.txt lab3b_check_sorted.txt
clean:
	rm -rf *.pyc
	rm -rf *_sorted.txt
	rm -rf lab3b_check.txt