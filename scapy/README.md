PCP extension for Scapy
=======================

The scapy_pcp.py defines new PCP layer to be used with Scapy.
You can find Scapy documentation on the following address
https://scapy.readthedocs.io/en/latest/

Quick intro
-----------

```bash
sudo ./scapy_pcp.py
```

This will start interactive mode in which PCP protocol extensions will be
loaded, and they can be used.

You can take a look at the fields available in PCP layer. Here is the
list of fields that you can edit in PCP for various opcodes:

```python
>>> (PCPRequest()/PCPMap()).show()
###[ PCP Request ]###
  version= v2
  R= request
  opcode= map
  reserved= ''
  lifetime= 1024
  source_ip= ::ffff:127.0.0.1
###[ PCP Map ]###
     nonce= '\xb9\xbc8`h\xdcv\xb9\x0c\xf6?\xdc'
     protocol= tcp
     reserved= ''
     int_port= 1234
     ext_port= 5678
     ext_ip= ::ffff:127.0.0.1
     \options\
    
>>> (PCPResponse()/PCPPeer(options=[PCPOption()/PCPOptionFilter()])).show2()
###[ PCP Response ]###
  version= v2
  R= response
  opcode= peer
  reserved1= ''
  result= success
  lifetime= 1024
  epoch= 0
  reserved2= ''
###[ PCP Peer ]###
     nonce= '\x12U.\xac\r\x9d!\r\xd5\xc8Q]'
     protocol= tcp
     reserved1= ''
     int_port= 1234
     ext_port= 5678
     ext_ip= ::ffff:127.0.0.1
     peer_port= 9101
     reserved2= ''
     peer_ip= ::ffff:127.0.0.1
     \options\
      |###[ PCP Option ]###
      |  code= filter
      |  reserved= ''
      |  length= 20
      |###[ PCP Filter Option ]###
      |     reserved= ''
      |     prefix_len= 0
      |     peer_port= 1234
      |     peer_ip= ::ffff:127.0.0.1
```

To see fields that can be set in PCP options you can use::

```python
>>> (PCPOption()/PCPOptionThirdParty()).show()
>>> (PCPOption()/PCPOptionPreferFailure()).show()
>>> (PCPOption()/PCPOptionFilter()).show()
```

You can use similar syntax to check the fields in other layers as well.

For example of IP or UDP layer::

```python
>>> ls(IP())
>>> IP().show()
```
Creating and sending PCP message
--------------------------------

To create PCP message you need to create IP and UDP layers and then add
PCP layer, with appropriate fields values set to certain values.

```python
>>> pkt = IP(dst="127.0.0.1")/UDP(sport=55555, dport=5351)/\
          PCPRequest(lifetime=120, source_ip="::ffff:127.0.0.1")/\
          PCPMap(int_port=2345)
```

The above command will create packet with destination 127.0.0.1 in IP header.
Source port at UDP layer will be 55555 and destination port will be port where
our PCP server is running.
You can change all the fields in PCP portion of the packet, so for example even
reserved fields can have custom value instead of default ZERO. This is ideal
for testing how PCP server handles various malformed requests.

To create packet with PCP option use `PCPOption()` to append desired option.
You can add multiple options simply by creating an array of multiple
`PCPOption()`:

```python
>>> pkt = IP(dst="127.0.0.1")/UDP(sport=55555, dport=5351)/\
          PCPRequest(lifetime=120, source_ip="::ffff:127.0.0.1")/\
          PCPMap(int_port=2345, options=[PCPOption()/PCPOptionPreferFailure(),
                                         PCPOption()/PCPOptionThirdParty(source_ip="::ffff:1.2.3.4")])
```


Sending packet is done using Scapy commands `sr()` or `sr1()`.

```python
>>> resp_pkt=sr1(pkt)
>>> resp_pkt.show()
```

You can then use resp_pkt to examine the response packet.

The return of `sr()` is little more complicated as it can send and receive
multiple packets. It returns two lists of packets, answered and unanswered.
To iterate over received pakcets in answered list, one would use something
similar to following code snippet::

```python
ans,unans = sr(d)
for snd,rcv in ans:
      rcv.show()
```

Sniffing packets with Scapy
---------------------------

You can sniff packets from Scapy session with PCP Addon, by using
`sniff()` function:

```python
Welcome to Scapy (2.2.0-dev)
PCP Addon
>>> packets=sniff(filter="udp port 5351", prn=lambda x: x.summary(), count=2)
Ether / IP / UDP 127.0.0.1:40167 > 127.0.0.1:5351 / PCP
>>> packets[1].show()
```

This will sniff two packets to list packets. You can then access the information
stored with `packets[1]` syntax and inspect the information in packets.

Fuzzing packets with Scapy
--------------------------

Scapy comes bundled with a simple fuzzer. To generate a random packet, wrap the
packet creation using the `fuzz()` function. This will generate random values
of the correct types for non-specified arguments:

```python
fuzzed_pkt = fuzz(PCPResponse(version=PCPVersions.V2)/PCPPeer(options=[PCPOption()/PCPOptionFilter()]))
>>> fuzzed_pkt.show2()
###[ PCP Response ]###
  version= v2
  R= request
  opcode= peer
  reserved1= '0'
  result= 66
  lifetime= 532197894
  epoch= 887128367
  reserved2= '0\xb1\xc3\xbea\x12\xef3\xca\xc2U;'
###[ PCP Peer ]###
     nonce= '\xdar\x14\x07\xc0\xec\xe5\x8c-\xceqo'
     protocol= 214
     reserved1= 'f\xcd\xb2'
     int_port= 62172
     ext_port= 47210
     ext_ip= 3b79:48b3:4613:5eb4:70f8:347:3614:a8df
     peer_port= 33563
     reserved2= '\ns'
     peer_ip= aaee:22e6:588e:8ae8:f09d:3f1b:2e6a:e687
     \options\
      |###[ PCP Option ]###
      |  code= filter
      |  reserved= '~'
      |  length= 20
      |###[ PCP Filter Option ]###
      |     reserved= '\xe5'
      |     prefix_len= 49
      |     peer_port= 56001
      |     peer_ip= 518e:19d0:3be7:c387:3bc:fc3b:6776:b941
```

As you can see, most of the essential semantics of the packet are preserved,
such as length, opcode, ... but other fields have been randomly mutated.
