# Builder installs toolchain; final stage is static binary only (scratch).
# Source file list must stay in sync with CMakeLists.txt (COMMON_SOURCES + queries_linux.c).
FROM alpine@sha256:48b0309ca019d89d40f670aa1bc06e426dc0931948452e8491e3d65087abc07d AS builder

RUN apk add --no-cache gcc musl-dev linux-headers

WORKDIR /src
COPY src/ src/
COPY CMakeLists.txt .
COPY VERSION .

RUN V=$(tr -d '[:space:]' < VERSION) && gcc -std=c17 -O2 -Wall -Wextra -Wno-unused-parameter \
    -Isrc/core -static \
    -flto \
    -fstack-protector-strong \
    -D_FORTIFY_SOURCE=2 \
    -Wformat -Wformat-security \
    -Wl,-z,relro,-z,now \
    -Wl,-z,noexecstack \
    -DDISK_LAYOUT_SCANNER_VERSION="\"$V\"" \
    -s \
    src/main.c \
    src/core/common.c \
    src/queries/queries_linux.c \
    src/output/text_output.c \
    src/output/json_output.c \
    src/output/html_output.c \
    -o /disk-layout-scanner && strip --strip-all /disk-layout-scanner

FROM scratch
LABEL org.opencontainers.image.title="disk-layout-scanner" \
    org.opencontainers.image.description="Cross-platform disk layout and storage identity reporting (static Linux binary)" \
    org.opencontainers.image.licenses="MIT"
COPY --from=builder /disk-layout-scanner /disk-layout-scanner
ENTRYPOINT ["/disk-layout-scanner"]
