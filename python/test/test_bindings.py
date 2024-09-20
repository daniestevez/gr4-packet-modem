import gr4_packet_modem
import pmtv

import time
import threading
import unittest
import sys


class TestBindings(unittest.TestCase):
    def test_global_block_registry(self):
        registry = gr4_packet_modem.globalBlockRegistry()

    def test_registry(self):
        registry = gr4_packet_modem.globalBlockRegistry()
        self.assertTrue(len(registry.providedBlocks()) > 0)
        self.assertEqual(registry.providedBlocks(),
                         registry.knownBlocks())
        for block in registry.providedBlocks():
            self.assertTrue(len(registry.knownBlockParameterizations(block))
                            > 0)
        name = registry.providedBlocks()[0]
        type_ = registry.knownBlockParameterizations(name)[0]
        block = registry.createBlock(name, type_, {})
        self.assertIsNotNone(block)

    def test_block_model(self):
        registry = gr4_packet_modem.globalBlockRegistry()
        block = registry.createBlock(
            'gr::packet_modem::NullSource', 'signed int', {})
        self.assertEqual(block.name(), 'gr::packet_modem::NullSource<int>')
        block.setName('my_block')
        self.assertEqual(block.name(), 'my_block')
        self.assertEqual(block.typeName(), 'gr::packet_modem::NullSource<int>')
        self.assertEqual(block.uniqueName(),
                         'gr::packet_modem::NullSource<int>#0')
        meta = block.metaInformation()
        settings = block.settings()

    def test_simple_flowgraph(self):
        sched = gr4_packet_modem.SchedulerSimpleSingleThreaded()
        fg = sched.graph()
        type_ = 'signed int'
        source = fg.emplaceBlock('gr::packet_modem::NullSource', type_, {})
        num_items = 100_000
        head = fg.emplaceBlock(
            'gr::packet_modem::Head', type_,
            {'num_items': gr4_packet_modem.pmt_from_uint64_t(num_items)})
        sink = fg.emplaceBlock('gr::packet_modem::NullSink', type_, {})
        fg.connect(source, 'out', head, 'in')
        fg.connect(head, 0, sink, 0)
        sched.runAndWait()

    def test_tag(self):
        tag = gr4_packet_modem.Tag()
        tag.index = 42
        tag.map = {'a': pmtv.pmt(17)}
        self.assertEqual(tag.index, 42)
        self.assertEqual(tag.map['a'](), 17)
        self.assertEqual(tag.at('a')(), 17)
        with self.assertRaises(IndexError):
            tag.at('b')
        self.assertEqual(tag.get('a')(), 17)
        self.assertIsNone(tag.get('b'))
        tag.insert_or_assign('c', pmtv.pmt(5))
        tag.insert_or_assign('a', pmtv.pmt(8))
        self.assertEqual(tag.map['a'](), 8)
        self.assertEqual(tag.map['c'](), 5)

    def test_stop_flowgraph(self):
        sched = gr4_packet_modem.SchedulerSimpleSingleThreaded()
        fg = sched.graph()
        type_ = 'signed int'
        source = fg.emplaceBlock('gr::packet_modem::NullSource', type_, {})
        sink = fg.emplaceBlock('gr::packet_modem::NullSink', type_, {})
        fg.connect(source, 'out', sink, 'in')

        to_scheduler = gr4_packet_modem.MsgPortOut()
        to_scheduler.connect(sched.msgIn)

        def stopper():
            time.sleep(1)
            gr4_packet_modem.sendMessage(gr4_packet_modem.Command.Set,
                                         to_scheduler,
                                         "",
                                         gr4_packet_modem.kLifeCycleState,
                                         {'state': pmtv.pmt('REQUESTED_STOP')})

        done = False

        def watchdog():
            time.sleep(3)
            if not done:
                print('watchdog')
                # use sys.exit() rather than raising an exception because
                # exceptions aren't caught anywhere
                sys.exit(1)

        thr = threading.Thread(target=stopper)
        thr.start()
        threading.Thread(target=watchdog, daemon=True).start()
        sched.runAndWait()
        done = True
        thr.join()


if __name__ == '__main__':
    unittest.main()
