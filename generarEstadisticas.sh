	#!/bin/bash

	cantidadDeProcesosMaximo=$1;

	for i in `seq 1 $cantidadDeProcesosMaximo`

	do
	start_time="$(date -u +%s.%N)"

	./procesoPadre /aplicaciones/filesIn/ /aplicaciones/filesOut/salida.txt i$ &

	process_id=$!;

	echo "el id del proceso padre recien lanzado es " $process_id;
	sleep 1;

	echo "se lanza vista que intercepta el proceso";

	./procesoVista $process_id

	end_time="$(date -u +%s.%N)"
	elapsed="$(bc <<<"$end_time-$start_time")"

	echo -e "Se tardo $elapsed segundos en procesar los archivos con $i procesos hijo \r\n">> ESTADISTICAS.txt



	done

