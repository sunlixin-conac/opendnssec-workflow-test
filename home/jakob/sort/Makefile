# $Id$

SORT=	sort -d

sort:
	$(SORT) < unsorted.txt >sorted.txt.tmp
	diff -u sorted.txt sorted.txt.tmp

clean:
	rm -f *.tmp
