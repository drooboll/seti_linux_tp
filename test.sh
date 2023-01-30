while :
do
	dd bs=2 count=1 if=/dev/adxl345-0 status=none | xxd
	sleep 0.01
done
