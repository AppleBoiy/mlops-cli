from setuptools import setup

setup(
    name="mops-tests",
    version="0.1.0",
    description="Python test suite dependencies for mops-cli",
    author="Chaipat J.",
    author_email="contact@chaipat.cc",
    packages=["tests"],
    install_requires=[
        "pytest>=7.0.0",
        "pytest-timeout>=2.1.0",
    ],
)
