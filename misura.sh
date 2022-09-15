#!/bin/bash

cd "output" ;
nclient=$(ls client* | wc -l) ; 
for (( i=1; i<=$nclient; i++)); do
	clientIDS[i]=$(cat client$i | grep SECRET | awk '{print $2}') ;
	clientSCRT[i]=$(cat client$i | grep SECRET | awk '{print $4}');
done

( cat supervisor | grep BASED ) > temp.txt
exec 3<temp.txt
corrette=0;
errore=0;

while read -u3 line; do

	stima=$(echo $line | awk '{print $3}') ;
	id=$(echo $line | awk '{print $5}') ;
	server=$(echo $line | awk '{print $7}') ;
	for (( i=1; i<$nclient; i++ )); do
		if [[ $id == ${clientIDS[i]} ]] ; then
			d=$(($stima-${clientSCRT[i]}))
			if [[ ($d -le 0 && $d -ge -25) ]]; then
				corrette=$(($corrette+1))
				errore=$(($errore-$d));
			elif [[ ($d -ge 0 && $d -le 25) ]] ; then
        		corrette=$(($corrette+1))
				errore=$(($errore+$d))	
			fi
		fi
	done	
done

rm temp.txt

echo "STIME CORRETTE : $corrette  $(echo "scale=2; $corrette/$nclient*100" | bc)% "
echo "ERRORE MEDIO COMMESSO : $(echo "scale=2; $errore/$nclient" | bc)"
