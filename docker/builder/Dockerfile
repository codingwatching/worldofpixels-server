FROM alpine
RUN apk add --no-cache gcc musl-dev make postgresql14-dev g++ openssl-dev zlib-dev curl-dev libpng-dev nlohmann-json inotify-tools
COPY watcher.sh /watcher.sh
WORKDIR /project
ENTRYPOINT ["/bin/sh", "/watcher.sh"]
