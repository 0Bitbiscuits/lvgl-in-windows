# 环境搭建

- 下载`msys2`

- 双击打开 `msys2\ucrt64.exe`
- 打开后依次执行这两条命令：

```
pacman -Syu
```
```
pacman -S --noconfirm mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-SDL2
mingw-w64-ucrt-x86_64-ninja
mingw-w64-ucrt-x86_64-make
mingw-w64-ucrt-x86_64-cmake
mingw-w64-ucrt-x86_64-pkgconf
```
如果不使用msys2中的cmake则需要额外执行命令 `pacman -S mingw-w64-ucrt-x86_64-pkg-config`

查看settings.json根据自身环境更新相关历经相关路径

