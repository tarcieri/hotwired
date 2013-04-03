#!/bin/sh

( echo "#!/bin/sh"; \
echo "exec `./host-config.sh CC` `./host-config.sh CFLAGS` -I. \${1+\"\$@\"}" \
) > compile

chmod +x compile
