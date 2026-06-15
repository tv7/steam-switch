"""Core logic for the multi-account Steam launcher.

Everything OS-agnostic lives here so the same code runs on Windows and Linux;
only `switcher.py` branches on platform for the actual account swap.
"""
