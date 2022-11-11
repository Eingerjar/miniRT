#include "structure.h"
#include "constant.h"

static int	*get_p_status(int type, void *object)
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
	int	type;

	type = obj.type;
	if (type == NONE || type == CAMERA || type == LIGHT)
		return ;
	status = get_p_status(obj.type, obj.object);
	if (*status & CHECKER)
		*status -= CHECKER;
	else
		*status += CHECKER;
}

void	apply_bump(t_obj obj)
{
	int	*status;
	int	type;

	type = obj.type;
	if (type == NONE || type == CAMERA || type == LIGHT)
		return ;
	status = get_p_status(obj.type, obj.object);
	if (*status & BUMP)
		*status -= BUMP;
	else
		*status += BUMP;
}

void	apply_img(t_obj obj)
{
	int	*status;
	int	type;

	type = obj.type;
	if (type == NONE || type == CAMERA || type == LIGHT)
		return ;
	status = get_p_status(obj.type, obj.object);
	if (*status & IMAGE)
		*status -= IMAGE;
	else
		*status += IMAGE;
}
