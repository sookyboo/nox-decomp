#!/bin/bash
docker rm noxdecomp_tmp-light || true
docker rm noxdecomp_tmp-x86-light || true

## Stop & remove unused containers/networks/images/build cache
#docker system prune -a --volumes
## Remove buildx cache across builders (safe default)
#docker buildx prune -a
## If you want it to run non-interactively:
#docker buildx prune -a -f

# light builds on disk space
docker load -i ../noxdecomp-build-arm64.tar
docker load -i ../noxdecomp-build-x86.tar

docker buildx build --platform=linux/arm64 --progress=plain -f Dockerfile.arm64-light -t noxdecomp-build-light . && \
docker create --name noxdecomp_tmp-light noxdecomp-build-light && \
docker cp noxdecomp_tmp-light:/build/nox-decomp/build/src/out ../noxd.armhf
# mkdir -p ../gl4es.armhf && \
# docker cp noxdecomp_tmp-light:/build/gl4es/lib/libGL.so.1 ../gl4es.armhf/libGL.so.1 && \
# mkdir -p ../ffmpeg.armhf && \
# docker cp noxdecomp_tmp-light:/opt/ffmpeg-armhf/lib/. ../ffmpeg.armhf/ && \
docker rm noxdecomp_tmp-light

docker buildx build --platform=linux/amd64 --progress=plain -f Dockerfile.x86-light -t noxdecomp-build-x86-light . &&  \
docker create --name noxdecomp_tmp-x86-light noxdecomp-build-x86-light && \
docker cp noxdecomp_tmp-x86-light:/build/nox-decomp/build/src/out ../noxd.i386
#docker cp noxdecomp_tmp-x86-light:/opt/ffmpeg-i386/lib/. ../ffmpeg.i386/ && \
docker rm noxdecomp_tmp-x86-light