# PThreadWorkerPool

A header-only thread automization library to make your multithreaded-lives easier.

The header is [this](https://github.com/AmmarkoV/PThreadWorkerPool/blob/main/pthreadWorkerPool.h), just copy it to your project and include it. You don't need to link it or worry about anything else (other than having pthreads and a C compiler), just include the header on your code.

I have also added an [example code snippet](https://github.com/AmmarkoV/PThreadWorkerPool/blob/main/example.c) that uses the library 


To build it use 

```
sudo apt-get install build-essential libpthread-stubs0-dev

make
```

After building it to test the example, use 

```
./example --threads 8 --iterations 64
```
