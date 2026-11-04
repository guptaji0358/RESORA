"""CLI entry point: `python -m scripts.engine <command> ...`."""
import sys

from .main import main

sys.exit(main())
