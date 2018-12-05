#!/bin/bash

cantidadDearchivos=$1;
cantidadDeCaracteresPorArchivo=$2;


for i in `seq 1 $cantidadDearchivos`

do
     cat /dev/urandom | tr -dc 'A-Za-z0-9!"#$%&'\''()*+,-./:;<=>?@[\]^_`{|}~' | fold -w $cantidadDeCaracteresPorArchivo | head -n 1 > "File$(printf "%03d" "$i").txt"
	 
	 echo archivo "File$(printf "%03d" "$i").txt" creado;
	 
done