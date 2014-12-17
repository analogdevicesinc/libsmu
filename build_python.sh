gcc -pthread -fno-strict-aliasing -march=x86-64 -mtune=generic -O2 -pipe -fstack-protector-strong --param=ssp-buffer-size=4 -DNDEBUG -march=x86-64 -mtune=generic -O2 -pipe -fstack-protector-strong --param=ssp-buffer-size=4 -fPIC -I/usr/include/python2.7 -lusb-1.0 -lm -c test.cpp -o build/temp.linux-x86_64-2.7/test.o -std=c++11
g++ -pthread -shared -Wl,-O1,--sort-common,--as-needed,-z,relro build/temp.linux-x86_64-2.7/test.o smu.a -lusb-1.0 -L/usr/lib  -lm -lpython2.7 -o build/lib.linux-x86_64-2.7/libsmu.so 

cp build/lib.linux-x86_64-2.7/libsmu.so ./
