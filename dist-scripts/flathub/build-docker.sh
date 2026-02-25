#!/bin/bash
docker buildx build --platform=linux/amd64 -f Dockerfile.flatpak -t localflatpak .
docker run -it --privileged localflatpak