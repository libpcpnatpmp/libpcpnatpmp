#!/usr/bin/env python
#
# Copyright (c) 2013 by Cisco Systems, Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice, this
#    list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
# ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
# ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
# SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


import os
from scapy.fields import *
from scapy.packet import bind_layers, Packet
from scapy.layers.inet import UDP
from scapy.layers.inet6 import IP6Field


class Dict2Struct(object):
    def __init__(self, entries):
        entries = dict((v.replace(' ', '_').upper(), k) for k, v in entries.items())
        self.__dict__.update(entries)


PCP_VERSIONS = {1: "v1",
                2: "v2"}
PCPVersions = Dict2Struct(PCP_VERSIONS)

PCP_MESSAGES = {0: "request",
                1: "response"}
PCPMessages = Dict2Struct(PCP_MESSAGES)

PCP_OPCODES = {0: "announce",
               1: "map",
               2: "peer"}
PCPOpcodes = Dict2Struct(PCP_OPCODES)

PCP_OPTIONS = {1: "third_party",
               2: "prefer_failure",
               3: "filter"}
PCPOptions = Dict2Struct(PCP_OPTIONS)

PCP_RESULTS = {0: "success",
               1: "unsupp_version",
               2: "not_authorized",
               3: "malformed_request",
               4: "unsupp_opcode",
               5: "unsupp_option",
               6: "malformed_option",
               7: "network_failure",
               8: "no_resources",
               9: "unsupp_protocol",
               10: "user_ex_quota",
               11: "cannot_provide_external",
               12: "address_mismatch",
               13: "excessive_remote_peers"}
PCPResults = Dict2Struct(PCP_RESULTS)

# Get here: https://www.iana.org/assignments/protocol-numbers/protocol-numbers-1.csv
PROTOCOL_NUMBERS = {0: "hopopt",
                    1: "icmp",
                    2: "igmp",
                    3: "ggp",
                    4: "ipv4",
                    5: "st",
                    6: "tcp",
                    7: "cbt",
                    8: "egp",
                    9: "igp",
                    10: "bbn-rcc-mon",
                    11: "nvp-ii",
                    12: "pup",
                    13: "argus (deprecated)",
                    14: "emcon",
                    15: "xnet",
                    16: "chaos",
                    17: "udp",
                    18: "mux",
                    19: "dcn-meas",
                    20: "hmp",
                    21: "prm",
                    22: "xns-idp",
                    23: "trunk-1",
                    24: "trunk-2",
                    25: "leaf-1",
                    26: "leaf-2",
                    27: "rdp",
                    28: "irtp",
                    29: "iso-tp4",
                    30: "netblt",
                    31: "mfe-nsp",
                    32: "merit-inp",
                    33: "dccp",
                    34: "3pc",
                    35: "idpr",
                    36: "xtp",
                    37: "ddp",
                    38: "idpr-cmtp",
                    39: "tp++",
                    40: "il",
                    41: "ipv6",
                    42: "sdrp",
                    43: "ipv6-route",
                    44: "ipv6-frag",
                    45: "idrp",
                    46: "rsvp",
                    47: "gre",
                    48: "dsr",
                    49: "bna",
                    50: "esp",
                    51: "ah",
                    52: "i-nlsp",
                    53: "swipe (deprecated)",
                    54: "narp",
                    55: "mobile",
                    56: "tlsp",
                    57: "skip",
                    58: "ipv6-icmp",
                    59: "ipv6-nonxt",
                    60: "ipv6-opts",
                    62: "cftp",
                    64: "sat-expak",
                    65: "kryptolan",
                    66: "rvd",
                    67: "ippc",
                    69: "sat-mon",
                    70: "visa",
                    71: "ipcv",
                    72: "cpnx",
                    73: "cphb",
                    74: "wsn",
                    75: "pvp",
                    76: "br-sat-mon",
                    77: "sun-nd",
                    78: "wb-mon",
                    79: "wb-expak",
                    80: "iso-ip",
                    81: "vmtp",
                    82: "secure-vmtp",
                    83: "vines",
                    84: "iptm",
                    85: "nsfnet-igp",
                    86: "dgp",
                    87: "tcf",
                    88: "eigrp",
                    89: "ospfigp",
                    90: "sprite-rpc",
                    91: "larp",
                    92: "mtp",
                    93: "ax.25",
                    94: "ipip",
                    95: "micp (deprecated)",
                    96: "scc-sp",
                    97: "etherip",
                    98: "encap",
                    100: "gmtp",
                    101: "ifmp",
                    102: "pnni",
                    103: "pim",
                    104: "aris",
                    105: "scps",
                    106: "qnx",
                    107: "a/n",
                    108: "ipcomp",
                    109: "snp",
                    110: "compaq-peer",
                    111: "ipx-in-ip",
                    112: "vrrp",
                    113: "pgm",
                    115: "l2tp",
                    116: "ddx",
                    117: "iatp",
                    118: "stp",
                    119: "srp",
                    120: "uti",
                    121: "smp",
                    122: "sm (deprecated)",
                    123: "ptp",
                    124: "isis over ipv4",
                    125: "fire",
                    126: "crtp",
                    127: "crudp",
                    128: "sscopmce",
                    129: "iplt",
                    130: "sps",
                    131: "pipe",
                    132: "sctp",
                    133: "fc",
                    134: "rsvp-e2e-ignore",
                    135: "mobility header",
                    136: "udplite",
                    137: "mpls-in-ip",
                    138: "manet",
                    139: "hip",
                    140: "shim6",
                    141: "wesp",
                    142: "rohc",
                    255: "reserved"}
ProtocolNumbers = Dict2Struct(PROTOCOL_NUMBERS)


class PacketNoPayload(Packet):
    # Taken from scapy-ssl_tls: https://github.com/tintinweb/scapy-ssl_tls/blob/master/scapy_ssl_tls/ssl_tls.py
    def extract_padding(self, s):
        return "", s


class PCPOption(Packet):
    name = "PCP Option"
    fields_desc = [ByteEnumField("code", PCPOptions.PREFER_FAILURE, PCP_OPTIONS),
                   StrFixedLenField("reserved", "\x00" * 1, 1),
                   LenField("length", None, fmt="H")]


class PCPOptionThirdParty(PacketNoPayload):
    name = "PCP Third Party Option"
    fields_desc = [IP6Field("source_ip", "::ffff:127.0.0.1")]


class PCPOptionPreferFailure(PacketNoPayload):
    name = "PCP Prefer Failure Option"
    fields_desc = []


class PCPOptionFilter(PacketNoPayload):
    name = "PCP Filter Option"
    fields_desc = [StrFixedLenField("reserved", "\x00" * 1, 1),
                   ByteField("prefix_len", 0),
                   ShortField("peer_port", 1234),
                   IP6Field("peer_ip", "::ffff:127.0.0.1")]


class PCPOptionUnknown(PacketNoPayload):
    name = "PCP Unknown Option"
    fields_desc = [StrField("data", "")]

    def _get_option_length(self, s):
        underlayer = self.underlayer
        if underlayer is not None and underlayer.name == PCPOption.name:
            option_len = underlayer[PCPOption].length
        else:
            option_len = len(s)
        return option_len

    def do_dissect(self, s):
        # Make do_dissect work only on the length of he current unknown option
        # If followed by further known or unknown options, they are handed over for further dissection by the upper
        # layers
        option_len = self._get_option_length(s)
        current_option = s[:option_len]
        next_option = s[option_len:]
        super(PCPOptionUnknown, self).do_dissect(current_option)
        return next_option


class PCPRequest(Packet):
    name = "PCP Request"
    fields_desc = [ByteEnumField("version", PCPVersions.V2, PCP_VERSIONS),
                   BitEnumField("R", PCPMessages.REQUEST, 1, PCP_MESSAGES),
                   BitEnumField("opcode", PCPOpcodes.MAP, 7, PCP_OPCODES),
                   StrFixedLenField("reserved", "\x00" * 2, 2),
                   IntField("lifetime", 0x400),
                   Emph(IP6Field("source_ip", "::ffff:127.0.0.1"))]


class PCPResponse(Packet):
    name = "PCP Response"
    fields_desc = [ByteEnumField("version", PCPVersions.V2, PCP_VERSIONS),
                   BitEnumField("R", PCPMessages.RESPONSE, 1, PCP_MESSAGES),
                   BitEnumField("opcode", PCPOpcodes.MAP, 7, PCP_OPCODES),
                   StrFixedLenField("reserved1", "\x00", 1),
                   ByteEnumField("result", PCPResults.SUCCESS, PCP_RESULTS),
                   IntField("lifetime", 0x400),
                   IntField("epoch", 0),
                   StrFixedLenField("reserved2", "\x00" * 12, 12)]

    def answers(self, other):
        if self.opcode == other.opcode:
            return self.payload.answers(other.payload)
        return False


class PCPMap(Packet):
    name = "PCP Map"
    fields_desc = [StrFixedLenField("nonce", os.urandom(12), 12),
                   ByteEnumField("protocol", ProtocolNumbers.TCP, PROTOCOL_NUMBERS),
                   StrFixedLenField("reserved", "\x00" * 3, 3),
                   Emph(ShortField("int_port", 1234)),
                   Emph(ShortField("ext_port", 5678)),
                   Emph(IP6Field("ext_ip", "::ffff:127.0.0.1")),
                   PacketListField("options", None, PCPOption)]

    def answers(self, other):
        return self.nonce == other.nonce


class PCPPeer(Packet):
    name = "PCP Peer"
    fields_desc = [StrFixedLenField("nonce", os.urandom(12), 12),
                   ByteEnumField("protocol", ProtocolNumbers.TCP, PROTOCOL_NUMBERS),
                   StrFixedLenField("reserved1", "\x00" * 3, 3),
                   Emph(ShortField("int_port", 1234)),
                   Emph(ShortField("ext_port", 5678)),
                   Emph(IP6Field("ext_ip", "::ffff:127.0.0.1")),
                   Emph(ShortField("peer_port", 9101)),
                   StrFixedLenField("reserved2", "\x00" * 2, 2),
                   Emph(IP6Field("peer_ip", "::ffff:127.0.0.1")),
                   PacketListField("options", None, PCPOption)]

    def answers(self, other):
        return self.nonce == other.nonce


bind_layers(UDP, PCPRequest, dport=5351)
bind_layers(UDP, PCPResponse, sport=5351)
bind_layers(PCPRequest, PCPMap, opcode=PCPOpcodes.MAP)
bind_layers(PCPResponse, PCPMap, opcode=PCPOpcodes.MAP)
bind_layers(PCPRequest, PCPPeer, opcode=PCPOpcodes.PEER)
bind_layers(PCPResponse, PCPPeer, opcode=PCPOpcodes.PEER)
bind_layers(PCPOption, PCPOptionThirdParty, code=PCPOptions.THIRD_PARTY)
bind_layers(PCPOption, PCPOptionPreferFailure, code=PCPOptions.PREFER_FAILURE)
bind_layers(PCPOption, PCPOptionFilter, code=PCPOptions.FILTER)
bind_layers(PCPOption, PCPOptionUnknown)

if __name__ == '__main__':
    from scapy.main import interact
    interact(mydict=globals(), mybanner="PCP Addon")
