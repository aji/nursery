#!/bin/sh

cd $(dirname $0)
if [[ ! -e ../a.out ]]; then
	cd ..
	make
	cd test
fi

for i in *.p; do
	echo -e "\e[1;34mfile: \e[1;33m$i"
	
	echo -e "\n\e[1;32m----SOURCE----\n"
	cat "$i"
	
	echo -e "\n\e[1;31m----OUTPUT----\n"
	../a.out < "$i"
	echo ""
done
