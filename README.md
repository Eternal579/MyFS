# MyFS
## 快速上手
以下均在source路径下执行
``` bash
dd bs=8K count=1k if=/dev/zero of=diskimg
make
./disk_init
mkdir mountDir
./MyFS -f mountDir
```