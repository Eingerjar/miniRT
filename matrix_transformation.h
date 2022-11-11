#ifndef MATRIX_TRANSFORMATION_H
# define MATRIX_TRANSFORMATION_H
# include "structure.h"

t_mat	get_rx_to_z(t_vec forward);
t_mat	get_ry_to_z(t_vec forward);
t_mat	mat_translation(double dx, double dy, double dz);
t_mat	rotate_latitude(int angle);
t_mat	rotate_longitude(int angle);
#endif