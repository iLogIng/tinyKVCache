# KV Cache

> cpp key-value cache

## GOAL 目标


## DESIGN 设计

> 使用底层 mmap 等api进行内存缓冲区域管理

### OPERATOR 操作

> op

***SEARCH***
- **all**
    - show all accessable kv
- **find**
    - find if has matched key

***CREATE***
- **put**
    - put a key-value into the kv engine

***READ***
- **get**
    - read a value by key when key is exist else return *null*
- **lst**
    - make a list of all key-value by key sork

***UPDATE***
- **chg**
    - change value of key who had already exist in this kv engine
- **load**
    - load a key-value cache file

***DELETE***
- **del**
    - delete a key-value but can find in *DELETED LIST*

### LOGIC 操作逻辑

---

## Feature Work

***BACKUP***
- **bak**
    - create a shortcut at the time for this system
***CONFIG***
- **net**
    - the ip:port config for network
- **req**
    - request remote server a value by key

### TYPE 类型

- **integer**
    - as signed 64bit
- **float**
    - as IEEE754 32/64bit
- **string**
    - string type
- **set**
    - non-type set of values
- **hash**
    - link to other stored key-value
- **link**
    - link to a file, could be an uri


---

## END

