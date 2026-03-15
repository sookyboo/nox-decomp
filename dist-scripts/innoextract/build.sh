docker buildx build \
  --platform=linux/amd64 \
  --target linux-amd64 \
  -f Dockerfile.innoextract \
  -o type=local,dest=./out/linux-x86_64 \
  .

docker buildx build \
  --platform=linux/arm64 \
  --target linux-arm64 \
  -f Dockerfile.innoextract \
  -o type=local,dest=./out/linux-aarch64 \
  .

docker buildx build \
  --platform=linux/amd64 \
  --target windows-amd64 \
  -f Dockerfile.innoextract \
  -o type=local,dest=./out/windows-x86_64 \
  .