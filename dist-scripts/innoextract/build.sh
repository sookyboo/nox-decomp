docker buildx build  \
  --target artifacts \
  -f Dockerfile \
  -o type=local,dest=./out \
  .