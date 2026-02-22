#!/bin/bash

## Stop & remove unused containers/networks/images/build cache
#docker system prune -a --volumes
## Remove buildx cache across builders (safe default)
#docker buildx prune -a
## If you want it to run non-interactively:
#docker buildx prune -a -f
docker context use default

# light builds on disk space
docker rm noxdecomp_tmp-light || true
docker load -i ../noxdecomp-build-arm64.tar
docker buildx build --platform=linux/arm64 --progress=plain -f Dockerfile.arm64-light -t noxdecomp-build-light . && \
docker create --name noxdecomp_tmp-light noxdecomp-build-light && \
docker cp noxdecomp_tmp-light:/build/nox-decomp/build/src/out ../noxd.armhf
# mkdir -p ../gl4es.armhf && \
# docker cp noxdecomp_tmp-light:/build/gl4es/lib/libGL.so.1 ../gl4es.armhf/libGL.so.1 && \
# mkdir -p ../ffmpeg.armhf && \
# docker cp noxdecomp_tmp-light:/opt/ffmpeg-armhf/lib/. ../ffmpeg.armhf/ && \
docker rm noxdecomp_tmp-light

docker rm noxdecomp_tmp-x86-light || true
docker load -i ../noxdecomp-build-x86.tar
docker buildx build --platform=linux/amd64 --progress=plain -f Dockerfile.x86-light -t noxdecomp-build-x86-light . &&  \
docker create --name noxdecomp_tmp-x86-light noxdecomp-build-x86-light && \
docker cp noxdecomp_tmp-x86-light:/build/nox-decomp/build/src/out ../noxd.i386
#docker cp noxdecomp_tmp-x86-light:/opt/ffmpeg-i386/lib/. ../ffmpeg.i386/ && \
docker rm noxdecomp_tmp-x86-light

docker context use default
docker load -i ../noxdecomp-build-win-x86.tar
docker rm noxdecomp_tmp-win-x86-light
docker buildx build --load --platform=linux/amd64 --progress=plain -f Dockerfile.winx86-light -t noxdecomp-build-win-x86-light . &&  \
docker create --name noxdecomp_tmp-win-x86-light noxdecomp-build-win-x86-light && \
rm -rf ../noxd.win32 && \
docker cp noxdecomp_tmp-win-x86-light:/build/nox-decomp/build/src/noxd.win32 ../noxd.win32 && \
# optional rename if you want a nicer exe name locally
( test -f ../noxd.win32/out.exe && mv -f ../noxd.win32/out.exe ../noxd.win32/noxd.winx86.exe || true ) && \
docker rm noxdecomp_tmp-win-x86-light