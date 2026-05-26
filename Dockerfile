FROM alpine:3.19 AS builder
RUN apk add --no-cache gcc musl-dev make
WORKDIR /build
COPY src/anarchy-s.c src/
COPY Makefile VERSION ./
RUN make

FROM alpine:3.19
LABEL org.opencontainers.image.title="anarchy-s"
LABEL org.opencontainers.image.description="Draw the Circle-A anarchy symbol in your terminal"
LABEL org.opencontainers.image.source="https://github.com/fourzerosix/anarchy-s"
LABEL org.opencontainers.image.licenses="MIT"
COPY --from=builder /build/anarchy-s /usr/local/bin/anarchy-s
ENTRYPOINT ["anarchy-s"]
CMD []
