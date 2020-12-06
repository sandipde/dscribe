from time import time
import numpy as np
from tqdm import tqdm
from ase import Atoms
from dscribe.descriptors import SOAP
import matplotlib.pyplot as mpl
import pkg_resources

# mpl.rcParams['text.usetex'] = True
# mpl.rcParams['font.family'] = 'serif'
# mpl.rcParams['font.serif'] = ['cm']
mpl.rcParams['axes.titlesize'] = 24
mpl.rcParams['xtick.labelsize'] = 16
mpl.rcParams['ytick.labelsize'] = 16
mpl.rcParams['axes.labelsize'] = 20
mpl.rcParams['legend.fontsize'] = 18

# Periodic test system.
a = 2.993
system_periodic = Atoms(
    "NiTi",
    positions=[[0.0, 0.0, 0.0], [a / 2, a / 2, a / 2]],
    cell=[a, a, a],
    pbc=True,
)

def test_soap(version):
    """Tests how the SOAP descriptor calculation scales with system size.
    """
    nmax = 4
    lmax = 4
    fig = mpl.figure(figsize=[9, 7])
    ax = fig.add_subplot(111)
    ax.set_title("SOAP nmax={}, lmax={}, version={}".format(nmax, lmax, version))
    ax.set_xlabel("Number of atoms")
    ax.set_ylabel("Time (s)")

    for rbf in ["gto", "polynomial"]:

        N = []
        t = []
        # Loop over different system sizes
        for ncells in tqdm(range(5, 15)):

            soap_generator = SOAP(rcut=3.0, nmax=nmax, lmax=lmax, species=["Ni", "Ti"], rbf=rbf, crossover=True, periodic=True)

            # Replicate system
            i_system = system_periodic.copy() * ncells
            t0 = time()
            soap_generator.create(i_system)
            t1 = time()
            N.append(len(i_system))
            t.append(t1 - t0)

        ax.plot(N, t, "o--", label="{}".format(rbf))

    mpl.legend()
    mpl.savefig("soap_scaling_{}.pdf".format(version))

def test_soap_derivatives(version):
    """Tests how the SOAP derivative calculation (numerical+analytica) scales
    with system size.
    """
    nmax = 4
    lmax = 4
    fig = mpl.figure(figsize=[9, 7])
    ax = fig.add_subplot(111)
    ax.set_title("SOAP derivatives nmax={}, lmax={}, version={}".format(nmax, lmax, version))
    ax.set_xlabel("Number of atoms")
    ax.set_ylabel("Time (s)")

    for method in ["numerical", "analytical"]:

        N = []
        t = []
        # Loop over different system sizes
        for ncells in tqdm(range(1, 8)):

            soap_generator = SOAP(
                rcut=3.0,
                nmax=nmax,
                lmax=lmax,
                species=["Ni", "Ti"],
                rbf="gto",
                crossover=True,
                periodic=True
            )

            # Extend system
            i_system = system_periodic.copy() * [ncells, ncells, 1]
            t0 = time()
            soap_generator.derivatives(i_system, method=method)
            t1 = time()
            N.append(len(i_system))
            t.append(t1 - t0)

        ax.plot(N, t, "o--", label="{}".format(method))

    mpl.legend()
    mpl.savefig("soap_derivatives_scaling_{}.pdf".format(version))

version = pkg_resources.get_distribution('dscribe').version
# test_soap(version)
test_soap_derivatives(version)
