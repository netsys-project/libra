# libra

Libra: A Building Block for In-network acceleration of Distributed Sparse Deep Learning

## PS-Lite + lwip

### Installation

1. Compile and install DPDK (17.11.10)
```
tar xvf dpdk-17.11.10.tar.xz
cd dpdk-stable-17.11.10
make install T=x86_64-native-linuxapp-gcc EXTRA_CFLAGS="-fPIC"
```

2. Compile and install libzmq + lwip
```
cd libzmq-4.2.3
mkdir build
cd build
cmake ..
make install
```

3. Build PS-Lite
```
mkdir build
cd build
cmake ..
make
```
