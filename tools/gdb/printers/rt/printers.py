import gdb

class Vector3Printer:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        x = self.val['x']
        y = self.val['y']
        z = self.val['z']
        return f"Vector3(x={x}, y={y}, z={z})"

def build_pretty_printer():
    pp = gdb.printing.RegexpCollectionPrettyPrinter("printers")
    pp.add_printer('Vector3', '^Vector3$', Vector3Printer)
    return pp

def register_rt_printers(obj):
    gdb.printing.register_pretty_printer(obj, build_pretty_printer())