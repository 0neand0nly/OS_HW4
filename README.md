# Operating Systems Homework3

* 21600162 Chanhwi Kim
* 21800637 Jooyoung Jang
* 21900699 Chanhwi Lee

[Assignment PDF]https://github.com/0neand0nly/OS_HW4/blob/main/homework4.pdf

---

# findeq: multithreaded search of files with equal data
 

findeq is a multithreaded program that recursively searches redudant files in the targeted directory and it's sub-directories. It will add the redudant file into a hash table which will eventually be printed to a file or STD_OUT upon the termination of this program.

# How to use this Program

### Preprocess

1. clone this repository using:

```
$ git clone https://github.com/0neand0nly/OS_HW4.git
```

2. Compile and run
   This repository provides example test programs. To run them, use ``$ make`` to compile and run. This will produce findeq exectuable file, and findeqD for debugging purposes

If you want to remove executable files, use
```
$ make clean
```

# Program Overview


* Example usage: USAGE: ./findeq -t= [numthreads] -m= [min_size of bytes] -o= [outputpath]\n DIR (directory to search)
  
$ sh ./findeq -t= 4 -m= 1024 -o= fileout.txt ./



```
s21800637@peace:~/HW/HW4$ ./findeq -t= 10 -m= 2000 -o= fileout.txt ./
Progress: 113 files processed
Execution Time: 7.03 seconds
File Written

Progress: 120 files processed
...
```

# Expected Output
```
fileout.txt:

[
	[
		".//Files/hw4/folder/subdir/symbolize_win32.inc"
	],
	[
		".//Files/hw4/fontello 2.svg"
	],
	[
		".//Files/hw4/newfolder/findeq.c"
	],
	[
		".//Files/hw4/folder/subdir/newdirectory/Arita-buriM-subset.woff"
	],
    ...

```


