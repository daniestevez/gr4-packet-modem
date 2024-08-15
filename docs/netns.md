# Network namespaces

gr4-packet-modem uses network namespaces to allow sending IP traffic through the
modem on a single machine. The required network namespaces and interfaces can be
set up by running `scripts/netns-setup`. These are organized as follows.

The modem TX gets packets from a `gr4_tun_tx` TUN interface in the `gr4_tx`
namespace. The modem RX writes packets to a `gr4_tun_tx` TUN interface in the
`gr4_rx` namespace.

Packets originating in `gr4_tx` with a destination IP address of `192.168.10.2`
get a source address of `192.168.10.1` and are sent to `gr4_tun_tx`, through the
modem TX, received by the modem RX, and written into `gr4_tun_rx`, where they
are processed by the `gr4_rx` namespace.

Packets originating in `gr4_rx` with a destination IP address of `192.168.10.1`
get a source address of `192.168.10.2` and are sent through `veth` interfaces to
the `gr4_tx` namespace. The interfaces are `v_rx` in the `gr4_rx` namespace and
`v_tx` in the `gr4_tx` namespace.

In summary, packets from `gr4_tx` with a destination of `192.168.10.2` are sent
over the modem, while packets from `gr4_rx` with a destination of `192.168.10.1`
are sent immediately over `veth` interfaces. In this way,

```
sudo ip netns exec gr4_tx ping 192.168.10.2
```

sends ping requests over the modem, while ping replies are immediately sent back
over `veth`, while

```
sudo ip netns exec gr4_rx ping 192.168.10.1
```

sends ping requests immediately over `veth` and ping replies through the
modem. In both cases, the latency measured by `ping` is the one-way latency
through the modem.
