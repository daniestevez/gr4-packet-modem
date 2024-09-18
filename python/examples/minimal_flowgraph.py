#!/usr/bin/env python3

import gr4_packet_modem

sched = gr4_packet_modem.SchedulerSimpleSingleThreaded()
fg = sched.graph()

type_ = 'float'
source = fg.emplaceBlock('gr::packet_modem::NullSource', type_, {})
head = fg.emplaceBlock(
    'gr::packet_modem::Head', type_,
    {'num_items': gr4_packet_modem.pmt_from_uint64_t(100_000_000_000)})
sink = fg.emplaceBlock('gr::packet_modem::NullSink', type_, {})

fg.connect(source, 'out', head, 'in')
fg.connect(head, 'out', sink, 'in')

sched.runAndWait()
