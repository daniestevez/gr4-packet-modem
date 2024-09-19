import gr4_packet_modem

import unittest


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


if __name__ == '__main__':
    unittest.main()
