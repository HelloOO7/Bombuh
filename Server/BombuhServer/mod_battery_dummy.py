from bomb import Bomb, VirtualModule, VariableType

class DummyBatteryModule(VirtualModule):
    
    def __init__(self, bomb: Bomb) -> None:
        super().__init__(bomb, "$DummyBattery")

    def list_variables(self) -> list[tuple]:
        return [
            ('Count', VariableType.STR_ENUM, ['None','None','None', '1','1', '2']),
        ]