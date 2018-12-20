#!/bin/bash
cantidadDeProcesosMaximo=$1;

#for i in `seq 1 $cantidadDeProcesosMaximo`

#do
start_time="$(date -u +%s.%N)"
./main2 test/ volcado1.txt $cantidadDeProcesosMaximo
process_id=$!;
echo "el id del proceso padre recien lanzado es " $process_id;
#sleep 1;

#echo "se lanza vista que intercepta el proceso";
#./vista $process_id
end_time="$(date -u +%s.%N)"
elapsed="$(bc <<<"$end_time-$start_time")"

echo -e "Se tardo $elapsed segundos en procesar los archivos con $cantidadDeProcesosMaximo procesos hijo \r\n">> ESTADISTICAS.txt
#done

