/*Copyright 2019 DScribe developers

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include "celllist.h"
#include "geometry.h"
#include <algorithm>
#include <limits>
#include <utility>
#include <map>
#include <utility>
#include <math.h>

using namespace std;

CellList::CellList(py::array_t<double> positions, double cutoff)
    : positions(positions)
    , cutoff(cutoff)
    , cutoff_squared(cutoff*cutoff)
{
    // Don't calculate distances if cutoff is zero
    if (cutoff == 0) {
    // For "infinite" cutoff we simply initialize a pairwise distance matrix.
    } else if (cutoff == numeric_limits<double>::infinity()) {
        this->init_distances();
    // For finite cutoff we initialize a cell list.
    } else {
        this->init_cell_list();
    }
}

void CellList::init_cell_list() {
    // Find cell limits
    auto pos_u = this->positions.unchecked<2>();
    this->xmin = this->xmax = pos_u(0, 0);
    this->ymin = this->ymax = pos_u(0, 1);
    this->zmin = this->zmax = pos_u(0, 2);
    for (ssize_t i = 0; i < pos_u.shape(0); i++) {
        double x = pos_u(i, 0);
        double y = pos_u(i, 1);
        double z = pos_u(i, 2);
        if (x < this->xmin) {
            this->xmin = x;
        };
        if (x > this->xmax) {
            this->xmax = x;
        };
        if (y < this->ymin) {
            this->ymin = y;
        };
        if (y > this->ymax) {
            this->ymax = y;
        };
        if (z < this->zmin) {
            this->zmin = z;
        };
        if (z > this->zmax) {
            this->zmax = z;
        };
    };

    // Add small padding to avoid floating point precision problems at the
    // boundary
    double padding = 0.0001;
    this->xmin -= padding;
    this->xmax += padding;
    this->ymin -= padding;
    this->ymax += padding;
    this->zmin -= padding;
    this->zmax += padding;

    // Determine amount and size of bins. The bins are made to be always of equal size.
    this->nx = max(1, int((this->xmax - this->xmin)/this->cutoff));
    this->ny = max(1, int((this->ymax - this->ymin)/this->cutoff));
    this->nz = max(1, int((this->zmax - this->zmin)/this->cutoff));
    this->dx = max(this->cutoff, (this->xmax - this->xmin)/this->nx);
    this->dy = max(this->cutoff, (this->ymax - this->ymin)/this->ny);
    this->dz = max(this->cutoff, (this->zmax - this->zmin)/this->nz);

    // Initialize the bin data structure. It is a 4D vector.
    this->bins = vector<vector<vector<vector<int>>>>(this->nx, vector<vector<vector<int>>>(this->ny, vector<vector<int>>(this->nz, vector<int>())));

    // Fill the bins with atom indices
    for (ssize_t idx = 0; idx < pos_u.shape(0); idx++) {
        double x = pos_u(idx, 0);
        double y = pos_u(idx, 1);
        double z = pos_u(idx, 2);

        // Get bin index
        int i = (x - this->xmin)/this->dx;
        int j = (y - this->ymin)/this->dy;
        int k = (z - this->zmin)/this->dz;

        // Add atom index to the bin
        this->bins[i][j][k].push_back(idx);
    };
}

void CellList::init_distances() {
    auto pos_u = positions.unchecked<2>();
    int n_atoms = pos_u.shape(0);

    vector<unordered_map<int, pair<double, double>>> results(n_atoms);
    for (int i = 0; i < n_atoms; ++i) {
        for (int j = i + 1; j < n_atoms; ++j) {
            double dx = pos_u(i, 0) - pos_u(j, 0);
            double dy = pos_u(i, 1) - pos_u(j, 1);
            double dz = pos_u(i, 2) - pos_u(j, 2);
            double distance_squared = dx*dx + dy*dy + dz*dz;
            double distance = sqrt(distance_squared);
            results[i][j] = make_pair(distance, distance_squared);
            results[j][i] = make_pair(distance, distance_squared);
        }
    }
    this->results = results;
}

unordered_map<int, pair<double, double>> CellList::getNeighboursForPosition(const double x, const double y, const double z) const
{
    unordered_map<int, pair<double, double>> result;
    auto pos_u = this->positions.unchecked<2>();

    // Get distances to all atoms if cutoff is infinite
    if (this->cutoff == numeric_limits<double>::infinity()) {
        int n_atoms = pos_u.shape(0);
        for (int i = 0; i < n_atoms; ++i) {
            double dx = x - pos_u(i, 0);
            double dy = y - pos_u(i, 1);
            double dz = z - pos_u(i, 2);
            double distance_squared = dx*dx + dy*dy + dz*dz;
            double distance = sqrt(distance_squared);
            result[i] = make_pair(distance, distance_squared);
        }
    // Otherwise use cell list to retrieve neighbours
    } else {
        // Find bin for the given position
        int i0 = (x - this->xmin)/this->dx;
        int j0 = (y - this->ymin)/this->dy;
        int k0 = (z - this->zmin)/this->dz;

        // Get the bin ranges to check for each dimension.
        int istart = max(i0-1, 0);
        int iend = min(i0+1, this->nx-1);
        int jstart = max(j0-1, 0);
        int jend = min(j0+1, this->ny-1);
        int kstart = max(k0-1, 0);
        int kend = min(k0+1, this->nz-1);

        // Loop over neighbouring bins
        for (int i = istart; i <= iend; i++){
            for (int j = jstart; j <= jend; j++){
                for (int k = kstart; k <= kend; k++){

                    // For each atom in the current bin, calculate the actual distance
                    vector<int> binIndices = this->bins[i][j][k];
                    for (auto &idx : binIndices) {
                        double ix = pos_u(idx, 0);
                        double iy = pos_u(idx, 1);
                        double iz = pos_u(idx, 2);
                        double deltax = x - ix;
                        double deltay = y - iy;
                        double deltaz = z - iz;
                        double distance_squared = deltax*deltax + deltay*deltay + deltaz*deltaz;
                        if (distance_squared <= this->cutoff_squared) {
                            result[idx] = make_pair(sqrt(distance_squared), distance_squared);
                        }
                    }
                }
            }
        }
    }
    return result;
}

unordered_map<int, pair<double, double>> CellList::getNeighboursForIndex(const int idx) const
{
    unordered_map<int, pair<double, double>> result;
    
    // Get distances to all atoms if cutoff is infinite
    if (this->cutoff == numeric_limits<double>::infinity()) {
        result = this->results[idx];
    // Otherwise use cell list to retrieve neighbours
    } else {
        auto pos_u = this->positions.unchecked<2>();
        double x = pos_u(idx, 0);
        double y = pos_u(idx, 1);
        double z = pos_u(idx, 2);
        result = this->getNeighboursForPosition(x, y, z);

        // Remove self from neighbours
        result.erase(idx);
    }
    return result;
}
