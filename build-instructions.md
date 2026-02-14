# PyCoreOS Build Instructions

## 1) Install `make` first

```bash
sudo apt-get update
sudo apt-get install -y make
```

## 2) Install project dependencies

```bash
make install-deps
```

## 3) Build the kernel

```bash
make build
```

## 4) Build the ISO

```bash
make iso
```

## 5) Run the OS

```bash
make run
```
