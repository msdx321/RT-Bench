CUR_PATH=`pwd`

# Tacle-Bench
BENCH_KERNEL=(binarysearch bitonic complex_updates countnegative deg2rad fft fir2dim insertsort jfdctint ludcmp md5 pm quicksort recursion st bitcount bsort cosf cubic fac filterbank iir isqrt lms matrix1 minver prime rad2deg sha)
BENCH_SEQUENTIAL=(adpcm_dec ammunition epic gsm_enc huff_dec mpeg2 petrinet rijndael_enc susan adpcm_enc cjpeg_transupp dijkstra fmref gsm_dec h264_dec huff_enc ndes rijndael_dec statemate)
BENCH_TEST=(cover duff test3)
# Compile with flag
cd ../../rt-tacle-bench
make CORE=CORE_I7
# Execute for all
cd bench/
## Kernel
cd kernel/
for bench in ${BENCH_KERNEL[@]}; do
	cd ${bench}
	sudo ./${bench} -d 0.1 -p 0.1 -l 2 -c 3 -t 50
	mv timing.csv ${CUR_PATH}/data/tacle/{bench}.csv
	cd ../
done
cd ../
## Sequential
cd sequential/
for bench in ${BENCH_SEQUENTIAL[@]}; do
        cd ${bench}
        sudo ./${bench} -d 0.1 -p 0.1 -l 2 -c 3 -t 50
        mv timing.csv ${CUR_PATH}/data/tacle/${bench}.csv
	cd ../
done
cd ../
## Test
cd test/
for bench in ${BENCH_TEST[@]}; do
        cd ${bench}
        sudo ./${bench} -d 0.1 -p 0.1 -l 2 -c 3 -t 50
        mv timing.csv ${CUR_PATH}/data/tacle/${bench}.csv
	cd ../
done
cd ../

# Vision (SD-VBS)
BENCH_VISION=(disparity localization mser sift stitch texture_synthesis tracking)
# Compile with flag
cd ../../vision/benchmarks/
for bench in ${BENCH_VISION[@]}; do
	echo ${bench}
	cd ${bench}/
	make CORE=CORE_I7
	cd ../
done
# Execute for all
for bench in ${BENCH_VISION[@]}; do
	cd ${bench}/data/vga/
	sudo ./${bench} -d 3 -p 3 -l 2 -c 3 -t 25 -b "."
	mv timing.csv ${CUR_PATH}/data/vision/${bench}.csv
	cd ../../../
done

# image-filters
BENCH_IMAGE_FILTERS=(gaussian_noise3  gaussian_noise5  gaussian_noise7  grayscale sepia  sobel  sobel5 threshold)
# Compile with flag
cd ../../image-filters
make CORE=CORE_I7
# Execute for all
for bench in ${BENCH_IMAGE_FILTERS[@]}; do
	sudo ./${bench} -d 3 -p 3 -l 2 -c 3 -t 25 -b "inputs/vga.bmp ${bench}.bmp"
	mv timing.csv ${CUR_PATH}/data/image-filters/${bench}.csv
done
