from pathlib import Path
import sys
from clocks import watchdog
if __name__=="__main__":watchdog(Path(sys.argv[1]))
