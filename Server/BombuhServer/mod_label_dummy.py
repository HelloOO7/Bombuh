from bomb import Bomb, VirtualModule, VariableType
import time

class DummyLabelModule(VirtualModule):
    
    def __init__(self, bomb: Bomb) -> None:
        super().__init__(bomb, "$DummyLabel")

    def list_variables(self) -> list[tuple[str, int]]:
        return [
            ('Text', VariableType.STR),
            ('IsLit', VariableType.BOOL)
        ]