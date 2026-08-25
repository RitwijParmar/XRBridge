FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends cmake ninja-build g++ ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DXRBRIDGE_BUILD_TESTS=OFF -DXRBRIDGE_BUILD_BENCHMARK=OFF \
 && cmake --build build --target xrbridge_demo --parallel

FROM debian:bookworm-slim
RUN useradd --system --uid 10001 xrbridge
WORKDIR /app
COPY --from=build /src/build/xrbridge_demo /app/xrbridge_demo
COPY --from=build /src/build/libxrbridge.so /app/libxrbridge.so
COPY demo/web /app/web
ENV LD_LIBRARY_PATH=/app PORT=8080
USER xrbridge
EXPOSE 8080
ENTRYPOINT ["/app/xrbridge_demo", "--assets", "/app/web"]
