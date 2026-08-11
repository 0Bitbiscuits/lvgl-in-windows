# 环境搭建

- 下载`msys2`

- 双击打开 `msys2\ucrt64.exe`
- 打开后依次执行这两条命令：

```
pacman -Syu
```
```
pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2 
mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-make 
mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-ninja
```

- 配置`msys/ucrt64/bin`到系统变量