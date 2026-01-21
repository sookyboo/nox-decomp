#!/bin/bash
docker rm noxdecomp_tmp || true
docker rm noxdecomp_tmp-x86 || true

docker buildx build --platform=linux/arm64 --progress=plain -f Dockerfile -t noxdecomp-build . && docker create --name noxdecomp_tmp noxdecomp-build && docker cp noxdecomp_tmp:/build/nox-decomp/build/src/out ../noxd.armhf && mkdir -p ../gl4es.armhf && docker cp noxdecomp_tmp:/build/gl4es/lib/libGL.so.1 ../gl4es.armhf/libGL.so.1 && mkdir -p ../ffmpeg.armhf && \
docker cp noxdecomp_tmp:/opt/ffmpeg-armhf/lib/. ../ffmpeg.armhf/ && \
docker rm noxdecomp_tmp

docker buildx build --platform=linux/amd64 --progress=plain -f Dockerfile.x86 -t noxdecomp-build-x86 . &&  \
docker create --name noxdecomp_tmp-x86 noxdecomp-build-x86 && \
docker cp noxdecomp_tmp-x86:/build/nox-decomp/build/src/out ../noxd.i386 && \
docker cp noxdecomp_tmp-x86:/opt/ffmpeg-i386/lib/. ../ffmpeg.i386/ && \
docker rm noxdecomp_tmp-x86