
image:image.c image.h
	gcc -g image.c -o image -lm
pthread:pthread.c image.h
	gcc -g pthread.c -o pthread -lm -pthread
openmp:openmp.c image.h
	gcc -g openmp.c -o openmp -lm -fopenmp
clean:
	rm -f image pthread openmp
