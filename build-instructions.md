# PyCoreOS Build Instructions

## 1) Install base tools

```bash
sudo apt-get update
sudo apt-get install -y python3 make
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

## 6) (Optional) Headless boot test

```bash
make test
```

## 7) (Optional) Build beta release bundle

```bash
make beta
```
