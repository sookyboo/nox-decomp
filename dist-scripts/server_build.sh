#docker buildx build --platform=linux/amd64 --progress=plain -f Dockerfile.x86 --load -t noxdecomp-server:amd64 .
#docker buildx build --platform=linux/arm64 --progress=plain -f Dockerfile.armhf --load -t noxdecomp-server:arm64 .

docker buildx create --use --name multiarch || docker buildx use multiarch

docker buildx build \
  --platform linux/amd64 \
  -f Dockerfile.x86 \
  -t ghcr.io/nox-decomp/nox-decomp-server:amd64 \
  --push \
  .

docker buildx build \
  --platform linux/arm64 \
  -f Dockerfile.armhf \
  -t ghcr.io/nox-decomp/nox-decomp-server:arm64 \
  --push \
  .

docker buildx imagetools create \
  -t ghcr.io/nox-decomp/nox-decomp-server:latest \
  ghcr.io/nox-decomp/nox-decomp-server:amd64 \
  ghcr.io/nox-decomp/nox-decomp-server:arm64
