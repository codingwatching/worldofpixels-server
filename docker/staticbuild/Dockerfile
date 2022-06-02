FROM alpine
RUN apk add --no-cache gcc musl-dev make postgresql14-dev g++ openssl-dev zlib-dev curl-dev libpng-dev nlohmann-json inotify-tools
WORKDIR /project
COPY . .
RUN make clean-all
RUN make -j $(nproc) -l $(nproc) ${MAKEOPTS:-all}
CMD ["/project/owopd"]