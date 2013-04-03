#!/bin/sh

( echo "#!/bin/sh"; \
echo "exec `./host-config.sh LD` `./host-config.sh LDFLAGS` \${1+\"\$@\"}" \
) > link

chmod +x link
