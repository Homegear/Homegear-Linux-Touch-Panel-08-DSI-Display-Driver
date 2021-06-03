#!/bin/sh

echo "Testing screen"


e=0
b=0

i=0
while [ "$i" -lt 3000 ]
do
    `xset s activate`
    `sleep 1`

    if cat /proc/hgltp08 | grep -q "1" ; then
	echo "BIG PROBLEM!"
	b=`expr $b + 1 `
    fi

    if  dmesg | tail -n 5 | grep -q DSI ; then
	echo "Problem!"
	e=`expr $e + 1`
    fi 


    `xset s reset`
    `sleep 1`
    i=`expr $i + 1`

    if cat /proc/hgltp08 | grep -q "1" ; then
	echo "BIG PROBLEM!"
	b=`expr $b + 1`
    fi

    if  dmesg | tail -n 5 | grep -q DSI ; then
	echo "Problem!"
	e=`expr $e + 1`
    fi 
done


echo "Counted $b BIG problems!"
echo "Counted $e small problems!"

