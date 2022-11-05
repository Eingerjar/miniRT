#include <stdio.h>
#include <fcntl.h>
#include "get_next_line.h"
#include "string_utils.h"
#include "parser_util_split.h"
#include <math.h>
#include "converter.h"
#include "checker.h"
#include "mlx.h"
#include <pthread.h>

#define	NONE		0
#define SPHERE		1
#define CYLINDER	2
#define PLANE		3
#define CONE		4
#define CAMERA		5
#define LIGHT		6
#define SHADOW_BIAS 1.00001
#define ESC_KEY		53
#define LEFT		123
#define UP			126
#define RIGHT		124
#define DOWN		125
#define P_WID 		1920
#define P_HEI		1080
#define BUMP_HIGHT_WEIGHT	100
#define BUMP_IMG	"bump.png"
#define THREAD		8
#define R_RAD		M_PI / 18
#define Q			12 // linux 113
#define E			14 // linux 101
#define W			97  // linux 97
#define A			115 // linux 115
#define S			113 // linux 113
#define D			101 // linux 101
#define C			8 // linux
#define B			11 // linux
#define I			34 // linux
#define CHECKER		1
#define IMAGE		2
#define BUMP		4
//#define	INFINITY	1e500
#define	S_EXP		32 // specular exponent
// .rt 파일에서 비어있는 줄에 공백이 들어가면 모든 내용이 올바른 형식으로 들어와도 invalid format이라고 뜨고 종료한다. 처리하고싶으면 처리하자.
typedef struct s_img
{
	void	*ptr;
	char	*addr;
	int		bits_per_pixel;
	int		line_length;
	int		endian;
	int		width;
	int		height;
}	t_img;

typedef struct s_obj
{
	int		type;
	void	*object;
	double	t;
}	t_obj;

typedef struct s_rgb
{
	int	r;
	int	g;
	int	b;
}	t_rgb;

typedef struct s_coordinate
{
	double	x;
	double	y;
	double	z;
}	t_coord;

typedef	t_coord	t_vec;

typedef struct s_obj_info
{
	t_coord	coord;
	t_vec	norm;
}	t_obj_info;

typedef struct s_ambient
{
	double	intensity;
	t_rgb	rgb;
}	t_ambient;

typedef struct s_light
{
	t_coord	coord;
	double	intensity;
	t_rgb	rgb;
}	t_light;

typedef struct s_camera
{
	t_coord	coord;
	t_vec	norm;
	int		fov;
}	t_camera;

typedef struct s_sphere
{
	t_coord	coord;
	double	diameter;
	t_rgb	rgb;
	int		status;
}	t_sp;

typedef struct s_plane
{
	t_coord	coord;
	t_vec	norm;
	t_rgb	rgb;
	int		status;
}	t_pl;

typedef struct s_cylinder
{
	t_coord	coord;
	t_vec	norm;
	double	diameter;
	double	height;
	t_rgb	rgb;
	int		status;
}	t_cy;

typedef t_cy	t_cn;

typedef struct s_node
{
	void			*data;
	struct s_node	*next;
}	t_node;

typedef struct s_world
{
	t_ambient	a;
	t_camera	c;
	t_light		l;
	t_node		*sp;
	t_node		*cy;
	t_node		*pl;
	t_node		*cn;
}	t_world;

typedef struct s_ray
{
	t_vec	pos;
	t_vec	dir;
}	t_ray;

typedef struct s_intersection
{
	double	l;
	double	r;
}	t_inter;

typedef struct s_equation
{
	double	a;
	double	b;
	double	c;
}	t_equation;

typedef struct s_vec4
{
	double	x;
	double	y;
	double	z;
	double	w;
}	t_vec4;

typedef struct s_mat
{
	t_vec4	r1;
	t_vec4	r2;
	t_vec4	r3;
	t_vec4	r4;
}	t_mat;

t_vec	get_basis_vec(t_vec);

typedef struct s_pixel_info
{
	t_coord	top_left;
	t_vec	p_h;
	t_vec	p_v;
}	t_p_info;

typedef struct s_vars
{
	void		*mlx;
	void		*win;
	t_img		img;
	t_img		b_img;
	t_obj		obj;
}	t_vars;

typedef struct s_thread_pram
{
	t_vars		*vars;
	t_world		*world;
	t_p_info	p_info;
	int			index;
	pthread_t	thread_id;
}	t_thread_pram;

int	check_rgb(t_rgb rgb)
{
	int	r;
	int	g;
	int	b;

	r = rgb.r;
	g = rgb.g;
	b = rgb.b;
	return (-1 < r && r < 256 && -1 < g && g < 256 && -1 < b && b < 256);
}

void	convert_color_to_grayscale(t_img *img)
{
	int				i;
	int				j;
	unsigned char	color;
	int				pixel;

	j = -1;
	while (++j < img->height)
	{
		i = -1;
		while (++i < img->width)
		{
			pixel = (i + j * img->width) * 4;
			color = 0.11 * (unsigned char)img->addr[pixel] + \
			0.59 * (unsigned char)img->addr[pixel + 1] + \
			0.3 * (unsigned char)img->addr[pixel + 2];
			img->addr[pixel] = color;
			img->addr[pixel + 1] = color;
			img->addr[pixel + 2] = color;
		}
	}
}

int	init_mlx_pointers(t_vars *vars)
{
	vars->mlx = mlx_init();
	if (!vars->mlx)
		return (0);
	vars->win = mlx_new_window(vars->mlx, P_WID, P_HEI, "miniRT");
	if (!vars->win)
		return (0);
	vars->img.ptr = mlx_new_image(vars->mlx, P_WID, P_HEI);
	if (!vars->img.ptr)
		return (0);
	vars->img.addr = mlx_get_data_addr(vars->img.ptr, &vars->img.bits_per_pixel,
			&vars->img.line_length, &vars->img.endian);
	if (!vars->img.addr)
		return (0);
	vars->b_img.ptr = mlx_png_file_to_image(vars->mlx, BUMP_IMG, \
					&vars->b_img.width, &vars->b_img.height);
	if (!vars->b_img.ptr)
		return (0);
	vars->b_img.addr = mlx_get_data_addr(vars->b_img.ptr, \
	&vars->b_img.bits_per_pixel, &vars->b_img.line_length, &vars->b_img.endian);
	if (!vars->b_img.addr)
		return (0);
	convert_color_to_grayscale(&vars->b_img);
	return (1);
}

int	check_normal(t_vec vec)
{
	double	x;
	double	y;
	double	z;

	x = vec.x;
	y = vec.y;
	z = vec.z;
	return (-1 <= x && x <= 1 && -1 <= y && y <= 1 && -1 <= z && z <= 1
		&& fabs(sqrt(x * x + y * y + z * z) - 1) < 1e-6);
}

int	free_splitted(char **splitted, int ret)
{
	int	i;

	i = -1;
	while (splitted[++i])
		free(splitted[i]);
	free(splitted);
	return (ret);
}

int	set_rgb(char *rgb_str, t_rgb *rgb)
{
	char	**rgb_info;
	int		rgb_info_cnt;

	if (!check_comma_cnt(rgb_str))
		return (0);
	rgb_info = split_line(rgb_str, ',', &rgb_info_cnt);
	if (rgb_info_cnt != 3
		|| !get_int(rgb_info[0], &(rgb->r))
		|| !get_int(rgb_info[1], &(rgb->g))
		|| !get_int(rgb_info[2], &(rgb->b)))
		return (free_splitted(rgb_info, 0));
	if (!check_rgb(*rgb))
		return (free_splitted(rgb_info, 0));
	return (free_splitted(rgb_info, 1));
}

int	set_coordinate(char *coord_str, t_coord *coord)
{
	char	**coord_info;
	int		coord_info_cnt;

	if (!check_comma_cnt(coord_str))
		return (0);
	coord_info = split_line(coord_str, ',', &coord_info_cnt);
	if (coord_info_cnt != 3
		|| !get_double(coord_info[0], &(coord->x))
		|| !get_double(coord_info[1], &(coord->y))
		|| !get_double(coord_info[2], &(coord->z)))
		return (free_splitted(coord_info, 0));
	return (free_splitted(coord_info, 1));
}

int	set_ambient(char **info, int cnt, t_world *world)
{
	double		intensity;
	t_ambient	*a;

	if (cnt != 3)
		return (0);
	a = &(world->a);
	if (!get_double(info[1], &intensity) || intensity < 0 || 1 < intensity)
		return (0);
	if (!set_rgb(info[2], &(a->rgb)))
		return (0);
	a->intensity = intensity;
	return (1);
}

int	set_light(char **info, int cnt, t_world *world)
{
	double	intensitiy;
	t_light	*l;

	if (cnt != 4)
		return (0);
	l = &(world->l);
	if (!set_coordinate(info[1], &(l->coord)))
		return (0);
	if (!get_double(info[2], &intensitiy) || intensitiy < 0 || 1 < intensitiy)
		return (0);
	if (!set_rgb(info[3], &(l->rgb)))
		return (0);
	l->intensity = intensitiy;
	return (1);
}

int	set_camera(char **info, int cnt, t_world *world)
{
	int			fov;
	t_camera	*c;

	if (cnt != 4)
		return (0);
	c = &(world->c);
	if (!set_coordinate(info[1], &(c->coord)))
		return (0);
	if (!set_coordinate(info[2], &(c->norm))
		|| !check_normal(c->norm))
		return (0);
	if (!get_int(info[3], &fov)	|| fov < 0 || 180 < fov) // 180 0
		return (0);
	c->fov = fov;
	return (1);
}

t_node	*get_last_node(t_node *list) // this function is called after malloc, so list is non-null.
{
	while (list->next)
		list = list->next;
	return (list);
}

int	set_plane(char **info, int cnt, t_world *world)
{
	t_pl	*pl;

	if (cnt != 4)
		return (0);
	pl = (t_pl *)(get_last_node(world->pl)->data);
	if (!set_coordinate(info[1], &(pl->coord)))
		return (0);
	if (!set_coordinate(info[2], &(pl->norm))
		|| !check_normal(pl->norm))
		return (0);
	if (!set_rgb(info[3], &(pl->rgb)))
		return (0);
	pl->status = 0;
	return (1);
}

int	set_sphere(char **info, int cnt, t_world *world)
{
	double	diameter;
	t_sp	*sp;

	if (cnt != 4)
		return (0);
	sp = (t_sp *)(get_last_node(world->sp)->data);
	if (!set_coordinate(info[1], &(sp->coord)))
		return (0);
	if (!get_double(info[2], &diameter) || diameter < 0)
		return (0);
	if (!set_rgb(info[3], &(sp->rgb)))
		return (0);
	sp->diameter = diameter;
	sp->status = 0;
	return (1);
}

int	set_cylinder(char **info, int cnt, t_world *world)
{
	double	diameter;
	double	height;
	t_cy	*cy;

	if (cnt != 6)
		return (0);
	cy = (t_cy *)(get_last_node(world->cy)->data);
	if (!set_coordinate(info[1], &(cy->coord)))
		return (0);
	if (!set_coordinate(info[2], &(cy->norm))
		|| !check_normal(cy->norm))
		return (0);
	if (!get_double(info[3], &diameter) || diameter < 0)
		return (0);
	if (!get_double(info[4], &height) || height < 0)
		return (0);
	if (!set_rgb(info[5], &(cy->rgb)))
		return (0);
	cy->diameter = diameter;
	cy->height = height;
	cy->status = 0;
	return (1);
}

int	set_cone(char **info, int cnt, t_world *world)
{
	double	diameter;
	double	height;
	t_cn	*cn;

	if (cnt != 6)
		return (0);
	cn = (t_cn *)(get_last_node(world->cn)->data);
	if (!set_coordinate(info[1], &(cn->coord)))
		return (0);
	if (!set_coordinate(info[2], &(cn->norm))
		|| !check_normal(cn->norm))
		return (0);
	if (!get_double(info[3], &diameter) || diameter < 0)
		return (0);
	if (!get_double(info[4], &height) || height < 0)
		return (0);
	if (!set_rgb(info[5], &(cn->rgb)))
		return (0);
	cn->diameter = diameter;
	cn->height = height;
	cn->status = 0;
	return (1);
}

int	alloc_sphere(void **ptr){
	t_sp	*sp;
	
	sp = (t_sp *)malloc(sizeof(t_sp));
	if (!sp)
		return (0);
	*ptr = (void *)sp;
	return (1);
}

int	alloc_plane(void **ptr){
	t_pl	*pl;
	
	pl = (t_pl *)malloc(sizeof(t_pl));
	if (!pl)
		return (0);
	*ptr = (void *)pl;
	return (1);
}

int	alloc_cylinder(void **ptr){
	t_cy	*cy;
	
	cy = (t_cy *)malloc(sizeof(t_cy));
	if (!cy)
		return (0);
	*ptr = (void *)cy;
	return (1);
}

int	alloc_cone(void **ptr)
{
	t_cn	*cn;

	cn = (t_cn *)malloc(sizeof(t_cn));
	if (!cn)
		return (0);
	*ptr = (void *)cn;
	return (1);
}

int	alloc_new_node(t_node **node, int index)
{
	static int	(*allocator[4])(void **)
		= {alloc_sphere, alloc_plane, alloc_cylinder, alloc_cone};

	*node = (t_node *)malloc(sizeof(t_node));
	if (!(*node))
		return (0);
	(*node)->next = 0;
	if (!(*allocator[index - 3])(&((*node)->data)))
	{
		free(*node);
		return (0);
	}
	return (1);
}

void	append_node(t_node **lst, t_node *new)
{
	if (!(*lst))
	{
		*lst = new;
		return ;
	}
	while ((*lst)->next)
		lst = &((*lst)->next);
	(*lst)->next = new;
}

void	clear_list(t_node **lst) // will be used in error handling function right before exit().
{
	t_node	*next;

	while (*lst)
	{
		next = (*lst)->next;
		free((*lst)->data);
		free(*lst);
		*lst = next;
	}
	*lst = 0;
}

int	set_object_list(t_world *rt, int index)
{
	t_node	**objects;
	t_node	*new_node;

	if (!alloc_new_node(&new_node, index))
		return (0);
	objects = &(rt->sp);
	if (index == 4)
		objects = &(rt->pl);
	else if (index == 5)
		objects = &(rt->cy);
	else if (index == 6)
		objects = &(rt->cn);
	append_node(objects, new_node);
	return (1);
}

/*
splitted
	1. 널 포인터 및 빈 문자열일 때 : 가드가 거름.
	2. 내용은 있지만, 유효하지 않은 상태일 때 : 각 set_* 함수들이 걸러야 함.
*/
int	set_world(char *line, t_world *rt, int *mask)
{
	char		**splitted;
	int			cnt;
	int			index;
	static int	(*fp[7])(char **, int, t_world *) = {set_ambient, set_camera,
		set_light, set_sphere, set_plane, set_cylinder, set_cone};
	
	splitted = split_line(line, ' ', &cnt); // all split_lines need null-guard.
	if (!(splitted && splitted[0]))
		return (free_splitted(splitted, 0));
	index = check_identifier(splitted[0]);
	if (index == -1)
		return (free_splitted(splitted, 0)); // caller must print error if set_world returns 0.
	if (index < 3 && (*mask & 1 << index || !(*fp[index])(splitted, cnt, rt))) // A, C, L shouldn't be on mutiple lines and don't need malloc.
		return (free_splitted(splitted, 0));
	*mask |= 1 << index;
	if (2 < index
		&& (!set_object_list(rt, index)	|| !(*fp[index])(splitted, cnt, rt)))
		return (free_splitted(splitted, 0));
	return (free_splitted(splitted, 1));
}

t_vec	vec_add(t_vec v1, t_vec v2)
{
	return ((t_vec){v1.x + v2.x, v1.y + v2.y, v1.z + v2.z});
}

t_vec	vec_sub(t_vec v1, t_vec v2)
{
	return ((t_vec){v1.x - v2.x, v1.y - v2.y, v1.z - v2.z});
}

t_vec	vec_scale(t_vec vec, double scalar)
{
	return ((t_vec){vec.x * scalar, vec.y * scalar, vec.z * scalar});
}

double	vec_dot(t_vec v1, t_vec v2)
{
	return (v1.x * v2.x + v1.y * v2.y + v1.z * v2.z);
}

t_vec	vec_cross(t_vec v1, t_vec v2)
{
	return ((t_vec){v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z,
		v1.x * v2.y - v1.y * v2.x});
}

double	vec_len(t_vec vec)
{
	return (sqrt(vec_dot(vec, vec)));
}

t_vec	vec_normalize(t_vec vec)
{
	double	length;

	length = vec_len(vec);
	return ((t_vec){vec.x / length, vec.y / length, vec.z / length});
}

t_vec	vec_proj(t_vec v1, t_vec v2)
{
	double	scalar;
	t_vec	n;

	n = vec_normalize(v2);
	scalar = vec_dot(v1, v2);
	return ((t_vec){n.x * scalar, n.y * scalar, n.z * scalar});
}

t_vec	vec_neg(t_vec vec)
{
	return ((t_vec){-vec.x, -vec.y, -vec.z});
}

double	choose_smaller_t(double current, double candidate, int condition)
{
	if (condition && candidate < current)
		return (candidate);
	return (current);
}

void	solve_equation(t_equation eq, t_inter *inter)
{
	double	d;

	d = eq.b * eq.b - 4 * eq.a * eq.c;
	if (d < 0)
	{
		inter->l = HUGE_VAL; // 
		inter->r = HUGE_VAL; // is it possible to use HUGE_VAL constant?
		return ;
	}
	inter->l = (-eq.b - sqrt(d)) / (2 * eq.a);
	inter->r = (-eq.b + sqrt(d)) / (2 * eq.a); // it is not necessary to compare l and r here.
}

int	intersect_sphere(t_ray ray, t_sp sp, double *t)// rename this to get_sphere_t and remove t_circle things.
{
	t_inter		inter;
	t_equation	eq;
	t_vec		ray_sp;
	double		ret;

	ray_sp = vec_sub(ray.pos, sp.coord);
	eq.a = vec_dot(ray.dir, ray.dir);
	eq.b = 2 * vec_dot(ray_sp, ray.dir);
	eq.c = vec_dot(ray_sp, ray_sp) - pow(sp.diameter / 2, 2);
	solve_equation(eq, &inter);
	ret = choose_smaller_t(HUGE_VAL, inter.l, (1 < inter.l));
	ret = choose_smaller_t(ret, inter.r, (1 < inter.r));
	*t = ret;
	return (ret != HUGE_VAL);
}

void	cal_cy_body(t_ray ray, t_cy cy, t_inter *inter)
{
	t_equation	eq;
	t_vec		vec_dir;
	t_vec		vec_pos;
	t_vec		cy_ray;

	vec_dir = vec_sub(ray.dir, vec_scale(cy.norm, vec_dot(ray.dir, cy.norm)));
	cy_ray = vec_sub(ray.pos, cy.coord);
	vec_pos = vec_sub(cy_ray, vec_scale(cy.norm, vec_dot(cy_ray, cy.norm)));
	eq.a = vec_dot(vec_dir, vec_dir);
	eq.b = 2 * vec_dot(vec_dir, vec_pos);
	eq.c = vec_dot(vec_pos, vec_pos) - pow(cy.diameter / 2, 2);
	solve_equation(eq, inter);
}

void	cal_cy_caps(t_ray ray, t_cy cy, t_inter *inter)
{
	t_vec	bottom_center;
	t_vec	top_center;
	double	n_dot_dir;

	bottom_center = cy.coord;
	top_center = vec_add(cy.coord, vec_scale(cy.norm, cy.height));
	n_dot_dir = vec_dot(cy.norm, ray.dir);
	inter->l = vec_dot(cy.norm, vec_sub(bottom_center, ray.pos)) / n_dot_dir;
	inter->r = vec_dot(cy.norm, vec_sub(top_center, ray.pos)) / n_dot_dir;
}

int	is_valid_t1t2(t_cy cy, t_ray ray, double t)
{
	return (t > 1 && vec_dot(cy.norm, vec_sub(
		vec_add(ray.pos, vec_scale(ray.dir, t)), cy.coord)) > 0
		&& vec_dot(cy.norm, vec_sub( 
		vec_add(ray.pos, vec_scale(ray.dir, t)),
		vec_add(cy.coord, vec_scale(cy.norm, cy.height)))) < 0);
}

int	is_valid_t3(t_cy cy, t_ray ray, double t)
{
	t_vec	q_to_cap;

	q_to_cap = vec_sub(vec_add(ray.pos, vec_scale(ray.dir, t)),
		cy.coord);
	return (t > 1 && vec_dot(q_to_cap, q_to_cap) < pow(cy.diameter / 2, 2));
}

int	is_valid_t4(t_cy cy, t_ray ray, double t)
{
	t_vec	q_to_cap;

	q_to_cap = vec_sub(vec_add(ray.pos, vec_scale(ray.dir, t)),
		vec_add(cy.coord, vec_scale(cy.norm, cy.height)));
	return (t > 1 && vec_dot(q_to_cap, q_to_cap) < pow(cy.diameter / 2, 2));
}
// t1 : nonnegative, vec_dot(cy.norm, vec_sub(q, p1)) > 0
// t2 : nonnegative, vec_dot(cy.norm, vec_sub(q, p2)) < 0
// t3 : nonnegative, vec_dot(vec_sub(q, p1), vec_sub(q, p1)) < r^2
// t4 : nonnegative, vec_dot(vec_sub(q, p2), vec_sub(q, p2)) < r^2
int	intersect_cylinder(t_ray ray, t_cy cy, double *t)// rename this to get_cylinder_t
{
	t_inter		t_1_2;
	t_inter		t_3_4;
	double		ret;

	cal_cy_body(ray, cy, &t_1_2);
	cal_cy_caps(ray, cy, &t_3_4);
	ret = choose_smaller_t(HUGE_VAL, t_1_2.l, is_valid_t1t2(cy, ray, t_1_2.l));
	ret = choose_smaller_t(ret, t_1_2.r, is_valid_t1t2(cy, ray, t_1_2.r));
	ret = choose_smaller_t(ret, t_3_4.l, is_valid_t3(cy, ray, t_3_4.l));
	ret = choose_smaller_t(ret, t_3_4.r, is_valid_t4(cy, ray, t_3_4.r));
	*t = ret;
	return (ret != HUGE_VAL);
}

int	intersect_plane(t_ray ray, t_pl pl, double *t) //rename this to get_plane_t
{
	double	ray_pl_dot;

	ray_pl_dot = vec_dot(ray.dir, pl.norm);
	if (fabs(ray_pl_dot) <= 1e-6)
		return (0);
	*t = vec_dot(vec_sub(pl.coord, ray.pos), pl.norm) / ray_pl_dot;
	return (*t > 1);
}

void	cal_cn_body(t_ray ray, t_cn cn, t_inter *inter)
{
	t_equation	eq;
	t_vec		vec_dir;
	t_vec		vec_pos;
	t_vec		cn_ray;
	double		theta;

	theta = pow((cn.diameter / 2.0) / cn.height, 2);
	vec_dir = vec_sub(ray.dir, vec_scale(cn.norm, vec_dot(ray.dir, cn.norm)));
	cn_ray = vec_sub(ray.pos, vec_add(cn.coord, vec_scale(cn.norm, cn.height)));
	vec_pos = vec_sub(cn_ray, vec_scale(cn.norm, vec_dot(cn_ray, cn.norm)));
	eq.a = vec_dot(vec_dir, vec_dir) - theta * pow(vec_dot(ray.dir, cn.norm), 2);
	eq.b = 2.0 * vec_dot(vec_dir, vec_pos) - \
		2.0 * theta * vec_dot(ray.dir, cn.norm) * vec_dot(cn_ray, cn.norm);
	eq.c = vec_dot(vec_pos, vec_pos) - theta * pow(vec_dot(cn_ray, cn.norm), 2);
	solve_equation(eq, inter);
}

void	cal_cn_caps(t_ray ray, t_cn cn, double *t3)
{
	double	n_dot_dir;

	n_dot_dir = vec_dot(cn.norm, ray.dir);
	*t3 = vec_dot(cn.norm, vec_sub(cn.coord, ray.pos)) / n_dot_dir;
}

int	is_valid_cn_t3(t_cn cn, t_ray ray, double t)
{
	t_vec	q_to_cap;

	q_to_cap = vec_sub(vec_add(ray.pos, vec_scale(ray.dir, t)),
		cn.coord);
	return (t > 1 && vec_dot(q_to_cap, q_to_cap) < pow(cn.diameter / 2, 2));
}

int	intersect_cone(t_ray ray, t_cn cn, double *t)
{
	t_inter		t_1_2;
	double		t3;
	double		ret;

	cal_cn_body(ray, cn, &t_1_2);
	cal_cn_caps(ray, cn, &t3);
	ret = choose_smaller_t(HUGE_VAL, t_1_2.l, is_valid_t1t2(cn, ray, t_1_2.l));
	ret = choose_smaller_t(ret, t_1_2.r, is_valid_t1t2(cn, ray, t_1_2.r));
	ret = choose_smaller_t(ret, t3, is_valid_cn_t3(cn, ray, t3));
	*t = ret;
	return (ret != HUGE_VAL);
}

void	check_sp(t_ray ray, t_node *sp, t_obj *obj)
{
	double	cur;
	double	t;

	if (!sp)
		return ;
	t = obj->t;
	while (sp)
	{
		if (intersect_sphere(ray, *((t_sp *)(sp->data)), &cur) && cur < t)
		{
			t = cur;
			obj->type = SPHERE;
			obj->object = (t_sp *)(sp->data);
		}
		sp = sp->next;
	}
	obj->t = t;
}

void	check_cy(t_ray ray, t_node *cy, t_obj *obj)
{
	double	cur;
	double	t;

	if (!cy)
		return ;
	t = obj->t;
	while (cy)
	{
		if (intersect_cylinder(ray, *((t_cy *)(cy->data)), &cur) && cur < t)
		{
			t = cur;
			obj->type = CYLINDER;
			obj->object = (t_cy *)(cy->data);
		}
		cy = cy->next;
	}
	obj->t = t;
}

void	check_pl(t_ray ray, t_node *pl, t_obj *obj)
{
	double	cur;
	double	t;

	if (!pl)
		return ;
	t = obj->t;
	while (pl)
	{
		if (intersect_plane(ray, *((t_pl *)(pl->data)), &cur) && cur < t)
		{
			t = cur;
			obj->type = PLANE;
			obj->object = (t_pl *)(pl->data);
		}
		pl = pl->next;
	}
	obj->t = t;
}

void	check_cn(t_ray ray, t_node *cn, t_obj *obj)
{
	double	cur;
	double	t;

	if (!cn)
		return ;
	t = obj->t;
	while (cn)
	{
		if (intersect_cone(ray, *((t_cn *)(cn->data)), &cur) && cur < t)
		{
			t = cur;
			obj->type = CONE;
			obj->object = (t_cn *)(cn->data);
		}
		cn = cn->next;
	}
	obj->t = t;
}

int	intersect(t_ray ray, t_world world, t_obj *obj)
{
	obj->type = 0;
	obj->t = INFINITY;
	check_sp(ray, world.sp, obj);
	check_cy(ray, world.cy, obj);
	check_pl(ray, world.pl, obj);
	check_cn(ray, world.cn, obj);
	return (obj->type);
}

int	check_shadow_sp(t_ray ray, t_node *sp)
{
	double	cur;

	cur = 2;
	if (!sp)
		return (0);
	while (sp)
	{
		if (intersect_sphere(ray, *((t_sp *)(sp->data)), &cur) && cur < 2)
			return (1);
		sp = sp->next;
	}
	return (0);
}

/////////////////////////////////////////////////////check_shadow
int	check_shadow_cy(t_ray ray, t_node *cy)
{
	double	cur;

	cur = 2;
	if (!cy)
		return (0);
	while (cy)
	{
		if (intersect_cylinder(ray, *((t_cy *)(cy->data)), &cur) && cur < 2)
			return (1);
		cy = cy->next;
	}
	return (0);
}

int	check_shadow_cn(t_ray ray, t_node *cn)
{
	double	cur;

	cur = 2;
	if (!cn)
		return (0);
	while (cn)
	{
		if (intersect_cone(ray, *((t_cn *)(cn->data)), &cur) && cur < 2)
			return (1);
		cn = cn->next;
	}
	return (0);
}

int	check_shadow_pl(t_ray ray, t_node *pl)
{
	double	cur;

	cur = 2;
	if (!pl)
		return (0);
	while (pl)
	{
		if (intersect_plane(ray, *((t_pl *)(pl->data)), &cur) && cur < 2)
			return (1);
		pl = pl->next;
	}
	return (0);
}

t_ray	get_l_ray(t_light l, t_vec p)
{
	t_ray	l_ray;

	l_ray.dir = vec_sub(l.coord, p);
	l_ray.pos = vec_sub(p, l_ray.dir);
	l_ray.dir = vec_scale(l_ray.dir, SHADOW_BIAS);
	return (l_ray);
}

int	check_shadow(t_world world, t_ray l_ray)
{
	if (check_shadow_sp(l_ray, world.sp))
		return (1);
	if (check_shadow_cy(l_ray, world.cy))
		return (1);
	if (check_shadow_pl(l_ray, world.pl))
		return (1);
	if (check_shadow_cn(l_ray, world.cn))
		return (1);
	return (0);
}

t_vec	rgb_to_vec(t_rgb rgb)
{
	return ((t_vec){rgb.r / 255.0, rgb.g / 255.0, rgb.b / 255.0});
}

t_rgb	mult_rgb_vec(t_rgb rgb, t_vec vec)
{
	return ((t_rgb){(int)(vec.x * rgb.r),
		(int)(vec.y * rgb.g), (int)(vec.z * rgb.b)});
}

t_vec	compute_diffuse(t_vec inter, t_vec n, t_world world)
{
	t_vec	ret;
	t_vec	l;
	double	n_dot_l;

	ret = (t_vec){0, 0, 0};
	l = vec_normalize(vec_sub(world.l.coord, inter));
	n_dot_l = vec_dot(n, l);
	if (n_dot_l > 0)
		ret = vec_scale(rgb_to_vec(world.l.rgb), world.l.intensity * n_dot_l);
	return (ret);
}

t_vec	compute_specular(t_vec inter, t_vec n, t_vec v, t_world world)
{
	t_vec	ret;
	t_vec	l;
	t_vec	r;
	double	r_dot_v;

	ret = (t_vec){0, 0, 0};
	l = vec_normalize(vec_sub(world.l.coord, inter));
	r = vec_sub(vec_neg(l), vec_scale(n, 2.0 * vec_dot(n, vec_neg(l))));
	r_dot_v = vec_dot(v, r);
	if (r_dot_v > 0)
		ret = vec_scale(rgb_to_vec(world.l.rgb),
			world.l.intensity * pow(r_dot_v, S_EXP));
    return (ret);
}

t_vec	compute_lighting(t_vec inter, t_vec n, t_vec v, t_world world) // only for diffuse reflection, p_norm should be an unit vector
{
	t_vec	v_ambient;
	t_vec	v_diffuse;
	t_vec	v_specular;
	t_vec	lighting;

	v_ambient = vec_scale(rgb_to_vec(world.l.rgb), world.a.intensity);
	if (check_shadow(world, get_l_ray(world.l, inter)))
		lighting = v_ambient;
	else
	{
		v_diffuse = compute_diffuse(inter, n , world);
		v_specular = compute_specular(inter, n, v, world);
		lighting = vec_add(vec_add(v_ambient, v_diffuse), v_specular);
	}
	lighting.x -= (lighting.x > 1.0) * (lighting.x - 1.0);
	lighting.y -= (lighting.y > 1.0) * (lighting.y - 1.0);
	lighting.z -= (lighting.z > 1.0) * (lighting.z - 1.0);
	return (lighting);
}

t_vec	get_viewport_vec(t_vec v)
{
	if (vec_len(vec_cross(v, (t_vec){0, 0, 1})) < 1e-6)
		return ((t_vec){1, 0, 0});
	return ((t_vec){0, 0, 1});
}

typedef struct s_uv
{
	double	u;
	double	v;
	t_vec	pu;
	t_vec	pv;
}	t_uv;

// u increases from 0 to 1 as you move counter-clockwise around the sphere
// v increases from 0 to 1 as you go from the north pole to the south pole.
t_uv	uv_map_sphere(t_coord p, t_sp sp)
{
	t_uv	uv;
	t_vec	vec;

	vec = vec_normalize(vec_sub(p, sp.coord));
	uv.u = 0.5 + atan2(vec.x, vec.y) / (2 * M_PI);
	uv.v = 0.5 - asin(vec.z) / M_PI;
	uv.pu = vec_normalize(vec_cross(vec, get_viewport_vec(vec)));
	uv.pv = vec_normalize(vec_cross(vec, uv.pu));
	return (uv);
}

t_uv	uv_map_plane(t_coord p, t_pl pl)
{
	t_vec	e1;
	t_vec	e2;

	e1 = vec_cross(pl.norm, get_basis_vec(pl.norm));
	e2 = vec_cross(pl.norm, e1);
	return ((t_uv){fabs(fmod(vec_dot(p, e1), 100)) / 100,
		fabs(fmod(vec_dot(p, e2), 100)) / 100,
		vec_normalize(e1),
		vec_normalize(e2)});
}

t_uv	uv_map_cylinder(t_coord p, t_cy cy)
{
	t_vec	e1;
	t_vec	e2;
	t_uv	uv;
	t_vec	c_p;
	t_vec	n_p;

	e1 = vec_cross(cy.norm, get_basis_vec(cy.norm));
	e2 = vec_cross(cy.norm, e1);
	c_p = vec_sub(p, cy.coord);
	uv.u = 0.5 + atan2(vec_dot(c_p, e1), vec_dot(c_p, e2)) / (2 * M_PI);
	uv.v = vec_len(vec_proj(vec_sub(p, cy.coord), cy.norm)) / cy.height;
	n_p = vec_normalize(vec_sub(p, vec_add(cy.coord, vec_proj(c_p, cy.norm))));
	if (vec_dot(c_p, cy.norm))
		n_p = cy.norm;
	uv.pu = vec_normalize(vec_cross(n_p, get_viewport_vec(n_p)));
	uv.pv = vec_normalize(vec_cross(n_p, uv.pu));
	return (uv);
}

t_vec	get_cone_body_norm(t_coord p, t_cn cn)
{
	t_coord	x;
	double	len_top_x;
	double	theta;
 
	theta = atan2(cn.diameter / 2, cn.height);
	len_top_x = vec_len(vec_sub(p, vec_add(p, vec_scale(cn.norm, cn.height)))) / cos(theta);
	x = vec_add(cn.coord, vec_scale(cn.norm, cn.height - len_top_x));
	return (vec_normalize(vec_sub(p, x)));
}

t_uv	uv_map_cone(t_coord p, t_cn cn)
{
	t_uv	ret;
	t_vec	n_p;

	ret = uv_map_cylinder(p, (t_cy)cn);
	n_p = get_cone_body_norm(p, cn);
	ret.pu = vec_normalize(vec_cross(n_p, get_viewport_vec(n_p)));
	ret.pv = vec_normalize(vec_cross(n_p, ret.pu));
	return (ret);
}

// u, v are in [0, 1]
char	uv_pattern_at(t_uv uv, int w, int h)
{
	if (((int)floor(uv.u * w) + (int)floor(uv.v * h)) % 2)
		return (1);
	return (0);
}

int	open_file(char *path)
{
	int	fd;
	int	len;

	fd = -1;
	len = ft_strlen(path);
	if (path[len - 3] == '.' && path[len - 2] == 'r' && path[len - 1] == 't')
		fd = open(path, O_RDONLY); // is read-only enough?
	// call error handling function to exit.
	return (fd);
}

int	read_file(int fd, t_world *world)
{
	char		*line;
	int			flag;
	static int	mask = 0; // bit-mask to check A, C, L should be declared only and at least once.

	flag = 1;
	while (flag)
	{
		line = get_next_line(fd);
		if (line == 0)
			break ;
		if (line[0] != '\0')
			flag = set_world(line, world, &mask);
		free(line);
	}
	if (!flag || !(mask & 1 << 0 && mask & 1 << 1 && mask & 1 << 2))
		return (0);
	return (1);
}

t_vec	get_basis_vec(t_vec v)
{
	t_vec	bx;
	t_vec	by;
	t_vec	bz;

	bx = (t_vec){1, 0, 0};
	by = (t_vec){0, 1, 0};
	bz = (t_vec){0, 0, 1};
	if (fabs(vec_dot(v, bx)) < 1e-6)
		return (bx);
	else if (fabs(vec_dot(v, bz)) < 1e-6)
		return (bz);
	return (by);
}

void	get_pixel_info(t_camera c, t_p_info *p_info)
{
	t_vec	h;
	t_vec	v;
	t_vec	tmp;
	double	vp_h;
	double	vp_w;

	vp_w = tan((c.fov * M_PI / 180.0) / 2.0) * 2;
	vp_h = vp_w * ((double)P_HEI / (double)P_WID);
	h = vec_cross(c.norm, get_viewport_vec(c.norm));
	v = vec_cross(c.norm, h);
	h = vec_scale(h, (double)1 / vec_len(h) * (vp_w / (double)P_WID));
	v = vec_scale(v, (double)1 / vec_len(v) * (vp_h / (double)P_HEI));
	p_info->p_h = h;
	p_info->p_v = v;
	tmp = vec_add(c.coord, c.norm);
	tmp = vec_sub(tmp, vec_scale(h, (double)P_WID / (double)2));
	tmp = vec_sub(tmp, vec_scale(v, (double)P_HEI / (double)2));
	p_info->top_left = tmp;
}

/////////////////////////////////////////////////////
t_ray	generate_ray(t_coord pos, t_p_info p_info, int i, int j)
{
	t_ray	ret;
	t_coord	tmp;

	ret.pos = pos;
	tmp = p_info.top_left;
	tmp = vec_add(tmp, vec_scale(p_info.p_h, i));
	tmp = vec_add(tmp, vec_scale(p_info.p_v, j));
	ret.dir = vec_sub(tmp, pos);
	return (ret);
}

t_rgb	get_img_rgb(t_img img, t_uv uv)
{
	int	i;

	i = img.width * uv.u + ((int)(img.height * uv.v)) * img.width;
	i *= 4;
	return ((t_rgb){(unsigned char)img.addr[i + 2], \
		(unsigned char)img.addr[i + 1], (unsigned char)img.addr[i]});
}

t_rgb	get_sp_rgb(t_img img, void *object, t_coord p)
{
	t_uv	uv;
	t_sp	*sp;

	sp = object;
	if ((sp->status & CHECKER))
	{
		uv = uv_map_sphere(p, *sp);
		if (uv_pattern_at(uv, 16, 16))
		{
			if (sp->status & IMAGE)
				return (get_img_rgb(img, uv));
			return (sp->rgb);
		}
		return ((t_rgb){255, 255, 255});
	}
	if (sp->status & IMAGE)
	{
		uv = uv_map_sphere(p, *sp);
		return (get_img_rgb(img, uv));
	}
	return (sp->rgb);
}

t_rgb	get_pl_rgb(t_img img, void *object, t_coord p)
{
	t_uv	uv;
	t_pl	*pl;

	pl = object;
	if ((pl->status & CHECKER))
	{
		uv = uv_map_plane(p, *pl);
		if (uv_pattern_at(uv, 16, 16))
		{
			if (pl->status & IMAGE)
				return (get_img_rgb(img, uv));
			return (pl->rgb);
		}
		return ((t_rgb){255, 255, 255});
	}
	if (pl->status & IMAGE)
	{
		uv = uv_map_plane(p, *pl);
		return (get_img_rgb(img, uv));
	}
	return (pl->rgb);
}

t_rgb	get_cy_rgb(t_img img, void *object, t_coord p)
{
	t_uv	uv;
	t_cy	*cy;

	cy = object;
	if ((cy->status & CHECKER))
	{
		uv = uv_map_cylinder(p, *cy);
		if (uv_pattern_at(uv, 16, 8))
		{
			if (cy->status & IMAGE)
				return (get_img_rgb(img, uv));
			return (cy->rgb);
		}
		return ((t_rgb){255, 255, 255});
	}
	if (cy->status & IMAGE)
	{
		uv = uv_map_cylinder(p, *cy);
		return (get_img_rgb(img, uv));
	}
	return (cy->rgb);
}

t_rgb	get_cn_rgb(t_img img, void *object, t_coord p)
{
	t_uv	uv;
	t_cn	*cn;

	cn = object;
	if ((cn->status & CHECKER))
	{
		uv = uv_map_cone(p, *cn);
		if (uv_pattern_at(uv, 16, 16))
		{
			if (cn->status & IMAGE)
				return (get_img_rgb(img, uv));
			return (cn->rgb);
		}
		return ((t_rgb){255, 255, 255});
	}
	if (cn->status & IMAGE)
	{
		uv = uv_map_cone(p, *cn);
		return (get_img_rgb(img, uv));
	}
	return (cn->rgb);
}

t_rgb	get_obj_rgb(t_vars *vars, t_obj obj, t_coord p, t_vec lighting)
{
	t_rgb			ret;
	static t_rgb	(*fp[4])(t_img, void *, t_coord) = {get_sp_rgb, \
		get_cy_rgb, get_pl_rgb, get_cn_rgb};

	ret = (*fp[obj.type - 1])(vars->b_img, obj.object, p);
	return (mult_rgb_vec(ret, lighting));
}

void	dot_pixel(t_img *img, t_rgb color, int i)
{
	int		pixel;

	pixel = i * 4;
	img->addr[pixel] = color.b;
	img->addr[pixel + 1] = color.g;
	img->addr[pixel + 2] = color.r;
}

t_vec	get_tangent_norm_sp(t_sp *sp, t_coord p, t_vec ray_dir)
{
	t_vec	n;

	n = vec_sub(p, sp->coord);
	if (vec_dot(ray_dir, n) > 0)
		return (vec_normalize(vec_sub(sp->coord, p)));
	return (vec_normalize(n));
}

t_vec	get_tangent_norm_cy(t_cy *cy, t_coord p, t_vec ray_dir)
{
	double	a;
	double	b;
	t_coord	t_c;

	a = vec_dot(p, cy->norm);
	b = vec_dot(vec_add(cy->coord, vec_scale(cy->norm, cy->height)), cy->norm);
	if (fabs(a - b) < 1e-6)
	{
		if (vec_dot(ray_dir, cy->norm) > 0)
			return (vec_scale(cy->norm, -1));
		return (cy->norm);
	}
	b = vec_dot(cy->coord, cy->norm);
	if (fabs(a - b) < 1e-6)
	{
		if (vec_dot(ray_dir, cy->norm) > 0)
			return (vec_scale(cy->norm, -1));
		return (cy->norm);
	}
	t_c = vec_add(cy->coord, vec_scale(cy->norm, fabs(a - b)));
	if (vec_dot(ray_dir, vec_sub(p, t_c)) > 0)
		return (vec_normalize(vec_sub(t_c, p)));
	return (vec_normalize(vec_sub(p, t_c)));
}

t_vec	get_tangent_norm_pl(t_pl *pl, t_vec ray_dir)
{
	if (vec_dot(ray_dir, pl->norm) > 0)
		return (vec_scale(pl->norm, -1));
	return (pl->norm);
}

t_vec	get_tangent_norm_cn(t_cn *cn, t_coord p, t_vec ray_dir)
{
	double	a;
	double	b;
	t_vec	ret;

	a = vec_dot(p, cn->norm);
	b = vec_dot(vec_add(cn->coord, vec_scale(cn->norm, cn->height)), cn->norm);
	if (fabs(a - b) < 1e-6)
	{
		if (vec_dot(ray_dir, cn->norm) > 0)
			return (vec_scale(cn->norm, -1));
		return (cn->norm);
	}
	b = vec_dot(cn->coord, cn->norm);
	if (fabs(a - b) < 1e-6)
	{
		if (vec_dot(ray_dir, cn->norm) > 0)
			return (vec_scale(cn->norm, -1));
		return (cn->norm);
	}
	ret = get_cone_body_norm(p, *cn);
	if (vec_dot(ray_dir, ret) > 0)
		return (vec_scale(ret, -1));
	return (ret);
}

t_vec	get_tangent_norm(t_obj	obj, t_coord p, t_vec ray_dir)
{
	t_vec	n;

	n = (t_vec){0, 0, 0};
	if (obj.type == SPHERE)
		n = get_tangent_norm_sp(obj.object, p, ray_dir);
	else if (obj.type == CYLINDER)
		n = get_tangent_norm_cy(obj.object, p, ray_dir);
	else if (obj.type == PLANE)
		n = get_tangent_norm_pl(obj.object, ray_dir);
	else if (obj.type == CONE)
		n = get_tangent_norm_cn(obj.object, p, ray_dir);
	return (n);
}

t_vec	get_bump_norm(t_img *b_img, t_obj obj, t_vec p, t_vec n)
{
	t_vec	ret;
	t_uv	uv;
	int		i;
	double	bu;
	double	bv;

	ret = (t_vec){0, 0, 0};
	if (obj.type == SPHERE)
		uv = uv_map_sphere(p, *(t_sp *)obj.object);
	else if (obj.type == CYLINDER)
		uv = uv_map_cylinder(p, *(t_cy *)obj.object);
	else if (obj.type == PLANE)
		uv = uv_map_plane(p, *(t_pl *)obj.object);
	else if (obj.type == CONE)
		uv = uv_map_cone(p, *(t_cn *)obj.object);
	i = b_img->width * uv.u + (int)(b_img->height * uv.v) * b_img->width;
	bu = b_img->addr[i * 4] - b_img->addr[(i + 1) * 4];
	bv = b_img->addr[i * 4] - b_img->addr[(b_img->width + i) * 4];
	bu /= BUMP_HIGHT_WEIGHT;
	bv /= BUMP_HIGHT_WEIGHT;
	ret = vec_sub(vec_scale(vec_cross(n, uv.pv), bu), \
			vec_scale(vec_cross(n, uv.pu), bv));
	return (vec_normalize(vec_add(n, ret)));
}

int	get_status(int	type, void *object)
{
	if (type == SPHERE)
		return ((t_sp *)object)->status;
	if (type == CYLINDER)
		return ((t_cy *)object)->status;
	if (type == PLANE)
		return ((t_pl *)object)->status;
	return ((t_cn *)object)->status;
}

int	trace_ray(t_vars *vars, t_world *world, t_ray ray, int i)
{
	t_obj	obj;
	t_rgb	color;
	t_coord	p;
	t_vec	n;

	if (!intersect(ray, *world, &obj))
		return (0);
	//background color
	p = vec_add(ray.pos, vec_scale(ray.dir, obj.t));
	n = get_tangent_norm(obj, p, ray.dir);
	if (get_status(obj.type, obj.object) & BUMP)
		n = get_bump_norm(&vars->b_img, obj, p, n);
	color = get_obj_rgb(vars, obj, p,
		compute_lighting(p, n, vec_neg(vec_normalize(ray.dir)), *world));
	dot_pixel(&vars->img, color, i);
	return (0);
}
/////////////////////////////////////////////////////

t_vec	vec_translate(t_vec v, double dx, double dy, double dz)
{
	return ((t_vec){v.x + dx, v.y + dy, v.z + dz});
}

t_vec	vec_rotate_v(t_vec forward)
{
	t_vec	up;
	t_vec	right;

	right = vec_normalize(vec_cross(forward, get_viewport_vec(forward)));
	up = vec_normalize(vec_cross(right, forward));
	return (vec_add(vec_scale(forward, cos(R_RAD)), vec_scale(up, sin(R_RAD))));
}

// forward and right are on a plane and up is it's forward.
// forward length is always 1.
t_vec	vec_rotate_h(t_vec forward)
{
	t_vec	right;

	right = vec_normalize(vec_cross(forward, get_viewport_vec(forward)));
	return (vec_add(vec_scale(forward, cos(R_RAD)), vec_scale(right, sin(R_RAD))));
}

void	*drawing(void *b_pram)
{
	t_thread_pram	pram;
	t_ray			ray;
	int				i;
	int				j;

	pram = *(t_thread_pram *)b_pram;
	j = pram.index * (P_HEI / THREAD) - 1;
	while (++j < (pram.index + 1) * (P_HEI / THREAD))
	{
		i = -1;
		while (++i < P_WID)
		{
			ray = generate_ray(pram.world->c.coord, pram.p_info, i, j);
			trace_ray(pram.vars, pram.world, ray, j * P_WID + i);
		}
	}
	return (0);
}

int draw_img(t_thread_pram *param)
{
	t_p_info	p_info;
	t_vars		*vars;
	void		*tmp;
	int			i;

	vars = param->vars;
	get_pixel_info(param->world->c, &p_info);
	i = -1;
	while (++i < THREAD)
	{
		param[i].p_info = p_info;
		if (pthread_create(&(param[i].thread_id), NULL, drawing, param + i))
			return (0);
	}
	i = -1;
	while (++i < THREAD)
		pthread_join(param[i].thread_id, &tmp);
	mlx_put_image_to_window(vars->mlx, vars->win, vars->img.ptr, 0, 0);
	return (1);
}


int	create_thread_pram(t_world *world, t_vars *vars, t_thread_pram **pram)
{
	t_thread_pram	*ret;
	int				i;

	ret = (t_thread_pram *)malloc(sizeof(t_thread_pram) * THREAD);
	if (ret == NULL)
		return (0);
	i = -1;
	while (++i < THREAD)
	{
		ret[i].vars = vars;
		ret[i].world = world;
		ret[i].index = i;
	}
	*pram = ret;
	return (1);
}

void	rotate_object(t_obj obj, int keycode)
{
	int			type;
	t_vec		rotated;
	t_obj_info	*info;

	type = obj.type;
	if (type == CYLINDER || type == PLANE || type == CONE || type == CAMERA)
	{
		info = (t_obj_info *)(obj.object);
		printf("info normal : <%f, %f, %f>\n", info->norm.x, info->norm.y, info->norm.z);
		printf("obj  normal : <%f, %f, %f>\n", ((t_cy *)(obj.object))->norm.x, ((t_cy *)(obj.object))->norm.y, ((t_cy *)(obj.object))->norm.z);
		if (keycode == Q)
			rotated = vec_rotate_v(info->norm);
		else
			rotated = vec_rotate_h(info->norm);
		info->norm = rotated;
		printf("info normal : <%f, %f, %f>\n", info->norm.x, info->norm.y, info->norm.z);
		printf("obj  normal : <%f, %f, %f>\n", ((t_cy *)(obj.object))->norm.x, ((t_cy *)(obj.object))->norm.y, ((t_cy *)(obj.object))->norm.z);
	}
}

int	*get_p_status(int type, void *object)
{
	if (type == SPHERE)
		return (&((t_sp *)object)->status);
	if (type == CYLINDER)
		return (&((t_cy *)object)->status);
	if (type == PLANE)
		return (&((t_pl *)object)->status);
	return (&((t_cn *)object)->status);
}

void	apply_checker(t_obj obj)
{
	int	*status;

	status = get_p_status(obj.type, obj.object);
	if (*status & CHECKER)
		*status -= CHECKER;
	else
		*status += CHECKER;
}

void	apply_bump(t_obj obj)
{
	int	*status;

	status = get_p_status(obj.type, obj.object);
	if (*status & BUMP)
		*status -= BUMP;
	else
		*status += BUMP;
}

void	apply_img(t_obj obj)
{
	int	*status;

	status = get_p_status(obj.type, obj.object);
	if (*status & IMAGE)
		*status -= IMAGE;
	else
		*status += IMAGE;
}

int	key_press_handler(int keycode, t_thread_pram *param)
{
	t_vars	*vars;

	vars = param->vars;
	printf("type is %d\n", vars->obj.type);
	printf("keycode is %d\n", keycode);
	if (vars->obj.type == NONE)
		return (0);
	if (keycode == Q || keycode == E)
		rotate_object(vars->obj, keycode);
	if (keycode == C)
		apply_checker(vars->obj);
	if (keycode == B)
		apply_bump(vars->obj);
	if (keycode == I)
		apply_img(vars->obj);
	// else if (keycode == W || keycode == A || keycode == D || keycode == D)
		// WASD should be one of the parameters of the function going to be called here.
	draw_img(param);
	return (0);
}

// int	main(void) //keycode tester
// {
// 	t_vars		vars;

// 	init_mlx_pointers(&vars);
// 	mlx_key_hook(vars.win, key_press_handler, &vars);
// 	mlx_loop(vars.mlx);
// }

int	main(int argc, char **argv)
{
	int				fd;
	t_world			world;
	t_vars			vars;
	t_thread_pram	*pram;

	if (argc != 2)
		return (0); // call error handling function with proper error message.
	fd = open_file(argv[1]);
	if (fd < 0) // remove this when error handling function is completed.
		return (0); // 에러 문자열 출력하고 처리해주기
	world.sp = 0;
	world.pl = 0;
	world.cy = 0;
	world.cn = 0;
	pram = NULL;
	if (read_file(fd, &world))
	{
		init_mlx_pointers(&vars);
		printf("%s\n", "valid format");
		if (!create_thread_pram(&world, &vars, &pram))
		{
			//free
			return (1);
		}
		if (!draw_img(pram))
		{
			//free
			return (1);
		}
		vars.obj.type = CYLINDER;
		vars.obj.object = world.cy->data;
		mlx_key_hook(vars.win, key_press_handler, pram);
		mlx_loop(vars.mlx);
	}
	else
		printf("%s\n", "invalid format");
	clear_list(&(world.sp));
	clear_list(&(world.pl));
	clear_list(&(world.cy));
	clear_list(&(world.cn));
	return (0);
}