#!/bin/bash
nameLength="${#3}"
name=$3
extraSpaces=$((20 - nameLength))
count=1
# pad the name with spaces so the number
# will appear on the second line of the VFD
while [[ $count -le extraSpaces ]]
do
    name+=" "
    (( count++ ))
done
# send a return
printf "\015" > /dev/ttyUSB0
sleep 1
# navigate the menu to instant message
printf "3" > /dev/ttyUSB0
sleep 1
# set the message
printf "$name $2" > /dev/ttyUSB0
# retrn 1 so mgetty does not answer the call
exit 1
