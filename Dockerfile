FROM alpine:3.19 AS builder
RUN apk add --no-cache gcc musl-dev make
WORKDIR /build
COPY anarchy-a/anarchy-a.c anarchy-a/
COPY Makefile VERSION ./
RUN make

FROM alpine:3.19
LABEL org.opencontainers.image.title="anarchy-a"
LABEL org.opencontainers.image.description="Draw the Circle-A anarchy symbol in your terminal"
LABEL org.opencontainers.image.source="https://github.com/fourzerosix/anarchy-a"
LABEL org.opencontainers.image.licenses="MIT"
COPY --from=builder /build/anarchy-a /usr/local/bin/anarchy-a
ENTRYPOINT ["anarchy-a"]
CMD []
