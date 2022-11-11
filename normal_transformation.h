#ifndef NORMAL_TRANSFORMATION_H
# define NORMAL_TRANSFORMATION_H
# include "structure.h"

t_vec	rotate_normal(t_vec vec, t_mat op);
t_vec	get_bump_norm(t_vars *vars, t_obj obj, t_vec p, t_vec n);
#endif