#include "stdafx.h"

GridField::GridField( Vector3f& grid_size, Vector3f& grid_min, Vector3i& grid_division )
{
	this->grid_size=grid_size;
	this->grid_min=grid_min;
	this->grid_division=grid_division;

	grids.resize(boost::extents[grid_division[0]][grid_division[1]][grid_division[2]]);
	for(int i=0;i<grids.shape()[0];i++)
		for(int j=0;j<grids.shape()[1];j++)
			for(int k=0;k<grids.shape()[2];k++)
				grids[i][j][k]=new GridNode();
}

GridField::GridField()
{

}

void GridField::clear()
{
	for(int i=0;i<grids.shape()[0];i++)
		for(int j=0;j<grids.shape()[1];j++)
			for(int k=0;k<grids.shape()[2];k++)
			{
				if(grids[i][j][k])
				{
					delete grids[i][j][k];
				}
			}
}

GridField::~GridField()
{
	clear();
}

bool MpmCore::inGrid( Vector3i& index, Vector3i& grid_division )
{
	if(index[0]>=0&&index[0]<grid_division[0]&&
		index[1]>=0&&index[1]<grid_division[1]&&
		index[2]>=0&&index[2]<grid_division[2])
		return true;
	return false;
}

float MpmCore::NX_bspline( float m )
{
	m=fabs(m);
	float ret;
	if(m<1.0f)
		ret=m*m*m*0.5-m*m+2.0/3.0;
	else if(m<2.0f)
		ret=m*m*m/(-6)+m*m-m*2 + 4.0/3.0;
	else
		return 0;

	if(ret<0.0000001f)
		return 0; 
	return ret;
}

float MpmCore::dNX_bsplineslope( float m )
{
	float abs_m=fabs(m);
	if(abs_m<1)
		return 1.5*m*abs_m-2*m;
	else if(abs_m<2)
		return -m*abs_m/2+m*2-2*m/abs_m;
	else
		return 0;
}

float MpmCore::weight( Vector3f& d_xp )
{
	return NX_bspline(d_xp[0])*NX_bspline(d_xp[1])*NX_bspline(d_xp[2]);
}

Vector3f MpmCore::weight_gradientF( Vector3f& d_xp )
{
	return Vector3f(
		dNX_bsplineslope(d_xp[0])*NX_bspline(d_xp[1])*NX_bspline(d_xp[2]),
		dNX_bsplineslope(d_xp[1])*NX_bspline(d_xp[2])*NX_bspline(d_xp[0]),
		dNX_bsplineslope(d_xp[2])*NX_bspline(d_xp[0])*NX_bspline(d_xp[1]));
}

void MpmCore::fromCinderToEigenMatrix33( Matrix3f& from, Eigen::Matrix3f& to )
{
	for(int i=0;i<3;i++)
		for(int j=0;j<3;j++)
		{
			to(i,j)=from(i,j);
		}
}

void MpmCore::fromEigenToCinderMatrix33( Eigen::Matrix3f& from, Matrix3f& to )
{
	for(int i=0;i<3;i++)
		for(int j=0;j<3;j++)
			to(i,j)=from(j,i);
}

Matrix3f MpmCore::cauchy_stress( Matrix3f& Fe, Matrix3f& Fp )
{
	Eigen::Matrix3f Fe_e;
	Eigen::Matrix3f Fp_e;
	fromCinderToEigenMatrix33(Fe, Fe_e);
	fromCinderToEigenMatrix33(Fp, Fp_e);

	float det_fe=Fe.fullPivLu().determinant();
	float det_fp=Fp.fullPivLu().determinant();

	//formula 2
	float miu=exp(ctrl_params.hardening*(1-det_fp))*ctrl_params.miu;
	float lambda=exp(ctrl_params.hardening*(1-det_fp))*ctrl_params.lambda;

	//polar decomposition FE=RE SE
	Eigen::JacobiSVD<Eigen::Matrix3f> svd(Fe_e, Eigen::ComputeFullU| Eigen::ComputeFullV);
	Eigen::Matrix3f Re_e=svd.matrixU()*svd.matrixV().transpose();
	Matrix3f Re;
	fromEigenToCinderMatrix33(Re_e, Re);

	//???? dx of fomular 1
	float sv = lambda*det_fe*(det_fe-1);
	Matrix3f second;
	second.setConstant(sv);
	//return (Fe-Re)*Fe.transpose()*2*miu + Matrix3f(lambda*det_fe*(det_fe-1));
	return (Fe-Re)*Fe.transpose()*2*miu + second;
}

void MpmCore::init_particle_volume_velocity()
{
	for(int pit=0;pit<particles.size();pit++)
	{
		Vector3f p_grid_index_f=particles[pit]->getGridIdx(grid->grid_min,grid->grid_size);
		Vector3i p_grid_index_i=particles[pit]->getGridIdx_int(grid->grid_min,grid->grid_size);
		for(int z=-2; z<=2;z++)
		{
			for(int y=-2; y<=2;y++)
			{
				for(int x=-2; x<=2;x++)
				{
					Vector3i index=p_grid_index_i+Vector3i(x,y,z);
					if(inGrid(index, grid->grid_division))
					{
						Vector3f xp=(particles[pit]->position-grid->grid_size.cwiseProduct(Vector3f(index[0],index[1],index[2]))-grid->grid_min).cwiseQuotient(grid->grid_size);
						float weight_p=weight(xp);
						grid->grids[index[0]][index[1]][index[2]]->mass+=weight_p*particles[pit]->pmass;
					}
				}
			}
		}
	}

	for(int pit=0;pit<particles.size();pit++)
	{
		float density=0.f;
		Vector3f p_grid_index_f=particles[pit]->getGridIdx(grid->grid_min,grid->grid_size);
		Vector3i p_grid_index_i=particles[pit]->getGridIdx_int(grid->grid_min,grid->grid_size);
		for(int z=-2; z<=2;z++)
		{
			for(int y=-2; y<=2;y++)
			{
				for(int x=-2; x<=2;x++)
				{
					Vector3i index=p_grid_index_i+Vector3i(x,y,z);
					if(inGrid(index, grid->grid_division))
					{
						Vector3f temp1=particles[pit]->position;
						Vector3f temp2=grid->grid_size.cwiseProduct(Vector3f(p_grid_index_i[0],p_grid_index_i[1],p_grid_index_i[2]))+grid->grid_min;
						Vector3f temp3=temp1-temp2;
						Vector3f temp4=temp3.cwiseQuotient(grid->grid_size);
						Vector3f xp=(particles[pit]->position-grid->grid_size.cwiseProduct(Vector3f(index[0],index[1],index[2]))-grid->grid_min).cwiseQuotient(grid->grid_size);
						float weight_p=weight(xp);
						float temp5=grid->grids[index[0]][index[1]][index[2]]->mass;
						density+=grid->grids[index[0]][index[1]][index[2]]->mass*weight_p/grid->grid_size[0]/grid->grid_size[1]/grid->grid_size[2];
					}
				}
			}
		}
		particles[pit]->volume=particles[pit]->pmass/density;
	}
}

void MpmCore::from_particles_to_grid()
{
	for(int x=0;x<grid->grids.shape()[0];x++)
		for(int y=0;y<grid->grids.shape()[1];y++)
			for(int z=0;z<grid->grids.shape()[2];z++)
			{
				grid->grids[x][y][z]->mass=0;
				grid->grids[x][y][z]->external_force = Vector3f(0,0,0);
				grid->grids[x][y][z]->velocity_old = Vector3f(0,0,0);
				grid->grids[x][y][z]->velocity_new = Vector3f(0,0,0);
				grid->grids[x][y][z]->active=false;
			}

			for(int pit=0;pit<particles.size();pit++)
			{
				Vector3f p_grid_index_f=particles[pit]->getGridIdx(grid->grid_min,grid->grid_size);
				Vector3i p_grid_index_i=particles[pit]->getGridIdx_int(grid->grid_min,grid->grid_size);

				Matrix3f cauchyStress=cauchy_stress(particles[pit]->Fe, particles[pit]->Fp)*particles[pit]->volume*-1;
				for(int z=-2; z<=2;z++)
				{
					for(int y=-2; y<=2;y++)
					{
						for(int x=-2; x<=2;x++)
						{
							Vector3i index=p_grid_index_i+Vector3i(x,y,z);
							if(inGrid(index, grid->grid_division))
							{
								Vector3f xp=(particles[pit]->position-grid->grid_size.cwiseProduct(Vector3f(index[0],index[1],index[2]))-grid->grid_min).cwiseQuotient(grid->grid_size);
								float weight_p=weight(xp);
								Vector3f gradient_weight=weight_gradientF(xp).cwiseQuotient(grid->grid_size);

								grid->grids[index[0]][index[1]][index[2]]->mass+=weight_p*particles[pit]->pmass;

								//now velocity is v*m. we need to divide m before use it
								grid->grids[index[0]][index[1]][index[2]]->velocity_old+=weight_p*particles[pit]->pmass*particles[pit]->velocity;
								//fomular 6
								grid->grids[index[0]][index[1]][index[2]]->external_force+=cauchyStress*gradient_weight;
							}
						}
					}
				}
			}
}

void MpmCore::compute_grid_velocity()
{
	//fstream fs("G://snow//log.txt",std::ios_base::app);
	//fs<<"frame "<<ctrl_params.frame<<":"<<endl;
	for(int x=0;x<grid->grids.shape()[0];x++)
		for(int y=0;y<grid->grids.shape()[1];y++)
			for(int z=0;z<grid->grids.shape()[2];z++)
			{
				if(grid->grids[x][y][z]->mass>0.f)
				{
					//as velocity is v*m in step 1. we need to divide m before use it
					grid->grids[x][y][z]->velocity_old/=grid->grids[x][y][z]->mass;
					grid->grids[x][y][z]->external_force+=ctrl_params.gravity*grid->grids[x][y][z]->mass;
					//v=v+f/m*deltaT
					grid->grids[x][y][z]->velocity_new=grid->grids[x][y][z]->velocity_old+grid->grids[x][y][z]->external_force/grid->grids[x][y][z]->mass*ctrl_params.deltaT;
					grid->grids[x][y][z]->active=true;
					//fs<<x<<","<<y<<","<<z<<":"<<grid->grids[x][y][z]->velocity_old[0]<<","<<grid->grids[x][y][z]->velocity_old[1]<<","<<grid->grids[x][y][z]->velocity_old[2]<<"|"<<
					//						//external_force_old[0]<<","<<external_force_old[1]<<","<<external_force_old[2]<<"|"<<
					//						grid->grids[x][y][z]->mass<<endl;

				}
			}
			//fs<<endl;
			//fs.flush();
			//fs.close();
}

bool MpmCore::getSDFNormal( Vector3f& grid_idx, Vector3f& out_sdf_normal )
{
	if(grid->grids[grid_idx[0]][grid_idx[1]][grid_idx[2]]->collision_sdf<0)
		return false;
	out_sdf_normal[0]=grid->grids[grid_idx[0]-1][grid_idx[1]][grid_idx[2]]->collision_sdf-grid->grids[grid_idx[0]+1][grid_idx[1]][grid_idx[2]]->collision_sdf;
	out_sdf_normal[1]=grid->grids[grid_idx[0]][grid_idx[1]-1][grid_idx[2]]->collision_sdf-grid->grids[grid_idx[0]][grid_idx[1]+1][grid_idx[2]]->collision_sdf;
	out_sdf_normal[2]=grid->grids[grid_idx[0]][grid_idx[1]][grid_idx[2]-1]->collision_sdf-grid->grids[grid_idx[0]][grid_idx[1]][grid_idx[2]+1]->collision_sdf;
	if(out_sdf_normal.norm()<=0.00000001)
		return false;
	out_sdf_normal.normalize();
	return true;
}

bool MpmCore::getSDFNormal_box( Vector3f& grid_idx_new, Vector3f& out_sdf_normal )
{
	out_sdf_normal= Vector3f(0,0,0);
	if(grid_idx_new[0]>=190)
		out_sdf_normal[0]=-1;
	if(grid_idx_new[0]<=10)
		out_sdf_normal[0]=1;
	if(grid_idx_new[1]>=190)
		out_sdf_normal[1]=-1;
	if(grid_idx_new[1]<=10)
		out_sdf_normal[1]=1;
	if(grid_idx_new[2]>=190)
		out_sdf_normal[2]=-1;
	if(grid_idx_new[2]<=10)
		out_sdf_normal[2]=1;
	if(out_sdf_normal.norm()<=0.00000001)
		return false;
	out_sdf_normal.normalize();
	return true;
}

bool MpmCore::updateVelocityWithSolvingCollision( Vector3f& collider_velocity, Vector3f& grid_velocity, Vector3f& sdf_normal, Vector3f& out_velocity )
{
	Vector3f v_rel=grid_velocity-collider_velocity;
	float vn=v_rel.dot(sdf_normal);
	if(vn>=0)
		return false;

	out_velocity=v_rel-sdf_normal*vn;
	float stickness=vn*ctrl_params.frictionCoeff;
	float out_v_length=out_velocity.norm();
	if(out_v_length<=-stickness)
		out_velocity=collider_velocity;//stick
	else
		out_velocity=out_velocity+out_velocity*stickness/out_v_length+collider_velocity;//friction
	return true;
}

void MpmCore::solve_grid_collision()
{
	for(int x=1;x<grid->grids.shape()[0]-1;x++)
		for(int y=1;y<grid->grids.shape()[1]-1;y++)
			for(int z=1;z<grid->grids.shape()[2]-1;z++)
			{
				if(!grid->grids[x][y][z]->active)
					continue;
				Vector3f new_grid=(grid->grids[x][y][z]->velocity_new*ctrl_params.deltaT).cwiseQuotient(grid->grid_size);
				new_grid+=Vector3f(x,y,z);
				Vector3f sdf_normal;
				if(getSDFNormal_box(new_grid,sdf_normal))
				{
					//fs<<x<<","<<y<<","<<z<<","<<sdf_normal[0]<<","<<sdf_normal[1]<<","<<sdf_normal[2]<<endl;
					Vector3f updated_v;
					if( updateVelocityWithSolvingCollision( grid->grids[x][y][z]->collision_velocity, grid->grids[x][y][z]->velocity_new, sdf_normal, updated_v) )
					{
						grid->grids[x][y][z]->velocity_new=updated_v;
					}
				}
			}
}

float MpmCore::clamp( float value, float low, float high )
{
	if(value < low)
		return low;
	if(value> high)
		return high;
	return value;
}

void MpmCore::compute_deformation_gradient_F()
{
	for(int pit=0;pit<particles.size();pit++)
	{
		Vector3f p_grid_index_f=particles[pit]->getGridIdx(grid->grid_min,grid->grid_size);
		Vector3i p_grid_index_i=particles[pit]->getGridIdx_int(grid->grid_min,grid->grid_size);

		Matrix3f velocity_gradient;
		velocity_gradient.setZero();
		Eigen::Matrix3f velocity_gradient_E;
		fromCinderToEigenMatrix33(velocity_gradient,velocity_gradient_E);
		for(int z=-2; z<=2;z++)
		{
			for(int y=-2; y<=2;y++)
			{
				for(int x=-2; x<=2;x++)
				{
					Vector3i index=p_grid_index_i+Vector3i(x,y,z);
					if(inGrid(index, grid->grid_division))
					{
						Vector3f xp=(particles[pit]->position-grid->grid_size.cwiseProduct(Vector3f(index[0],index[1],index[2]))-grid->grid_min).cwiseQuotient(grid->grid_size);
						Vector3f gradient_weight=weight_gradientF(xp);
						//fomular in step 4
						velocity_gradient_E+=Eigen::Vector3f(grid->grids[index[0]][index[1]][index[2]]->velocity_new[0],grid->grids[index[0]][index[1]][index[2]]->velocity_new[1],grid->grids[index[0]][index[1]][index[2]]->velocity_new[2])*
							Eigen::Vector3f(gradient_weight[0],gradient_weight[1],gradient_weight[2]).transpose();
					}
				}
			}
		}

		Eigen::Matrix3f Fe_p_E;
		fromCinderToEigenMatrix33(particles[pit]->Fe,Fe_p_E);
		Eigen::Matrix3f Fp_p_E;
		fromCinderToEigenMatrix33(particles[pit]->Fp,Fp_p_E);

		//fomular 11
		Eigen::Matrix3f Fe_new=(Eigen::Matrix3f::Identity()+velocity_gradient_E*ctrl_params.deltaT)*Fe_p_E;
		Eigen::Matrix3f F_new=Fe_new*Fp_p_E;
		Eigen::JacobiSVD<Eigen::Matrix3f> svd(Fe_new, Eigen::ComputeFullV | Eigen::ComputeFullU);

		Matrix3f clamped_S;
		clamped_S.setZero();
		clamped_S(0,0)=clamp(svd.singularValues()(0), 1-ctrl_params.critical_compression, 1+ ctrl_params.critical_stretch);
		clamped_S(1,1)=clamp(svd.singularValues()(1), 1-ctrl_params.critical_compression, 1+ ctrl_params.critical_stretch);
		clamped_S(2,2)=clamp(svd.singularValues()(2), 1-ctrl_params.critical_compression, 1+ ctrl_params.critical_stretch);

		Matrix3f clamped_S_inv;
		clamped_S_inv.setZero();
		clamped_S_inv(0,0)=1/clamped_S(0,0);
		clamped_S_inv(1,1)=1/clamped_S(1,1);
		clamped_S_inv(2,2)=1/clamped_S(2,2);

		Matrix3f U;
		Eigen::Matrix3f U_E=svd.matrixU();
		fromEigenToCinderMatrix33(U_E, U);
		Matrix3f V;
		Eigen::Matrix3f V_E=svd.matrixV();
		fromEigenToCinderMatrix33(V_E, V);
		Matrix3f F_new_cinder;
		fromEigenToCinderMatrix33(F_new, F_new_cinder);

		//fomular 12
		particles[pit]->Fe=U*clamped_S*V.transpose();
		particles[pit]->Fp=V*clamped_S_inv*U.transpose()*F_new_cinder;
	}
}

void MpmCore::from_grid_to_particle()
{
	for(int pit=0;pit<particles.size();pit++)
	{
		//Vector3f p_grid_index_f=particles[pit]->getGridIdx(grid->grid_center,grid->grid_size);
		Vector3i p_grid_index_i=particles[pit]->getGridIdx_int(grid->grid_min,grid->grid_size);
		Vector3f v_PIC(0,0,0);
		Vector3f v_FLIP=particles[pit]->velocity;
		//Matrix3f cauchyStress=cauchy_stress(particles[pit]->Fe, particles[pit]->Fp)*particles[pit]->volume*-1;
		for(int z=-2; z<=2;z++)
		{
			for(int y=-2; y<=2;y++)
			{
				for(int x=-2; x<=2;x++)
				{
					Vector3i index=p_grid_index_i+Vector3i(x,y,z);
					if(inGrid(index, grid->grid_division))
					{
						Vector3f xp=(particles[pit]->position-grid->grid_size.cwiseProduct(Vector3f(index[0],index[1],index[2]))-grid->grid_min).cwiseQuotient(grid->grid_size);
						float weight_p=weight(xp);
						v_PIC+=grid->grids[index[0]][index[1]][index[2]]->velocity_new*weight_p;
						v_FLIP+=(grid->grids[index[0]][index[1]][index[2]]->velocity_new-grid->grids[index[0]][index[1]][index[2]]->velocity_old)*weight_p;
						if(pit==0&&ctrl_params.frame>=1)
						{
							//fs<<"index:"<<index[0]<<" "<<index[1]<<" "<<index[2]<<endl;
							//fs<<"vel_new:"<<grid->grids[index[0]][index[1]][index[2]]->velocity_new[0]<<" "<<grid->grids[index[0]][index[1]][index[2]]->velocity_new[1]<<" "<<grid->grids[index[0]][index[1]][index[2]]->velocity_new[2]<<endl;
							//fs<<"vel_old:"<<grid->grids[index[0]][index[1]][index[2]]->velocity_old[0]<<" "<<grid->grids[index[0]][index[1]][index[2]]->velocity_old[1]<<" "<<grid->grids[index[0]][index[1]][index[2]]->velocity_old[2]<<endl;
						}
					}
				}
			}
		}
		particles[pit]->velocity=v_PIC*(1-ctrl_params.flip_percent)+v_FLIP*ctrl_params.flip_percent;
		if(pit==0)
			cout<<"vel:"<<particles[pit]->velocity[0]<<" "<<particles[pit]->velocity[1]<<" "<<particles[pit]->velocity[2]<<endl;
	}
}

void MpmCore::solve_particle_collision()
{
	for(int pit=0;pit<particles.size();pit++)
	{
		Vector3f p_grid_index_f=particles[pit]->getGridIdx(grid->grid_min,grid->grid_size);
		Vector3i p_grid_index_i=particles[pit]->getGridIdx_int(grid->grid_min,grid->grid_size);
		Vector3f p_grid_index_i_new=(particles[pit]->position+particles[pit]->velocity*ctrl_params.deltaT-grid->grid_min).cwiseQuotient(grid->grid_size);

		float sdf=0;
		Vector3f sdf_normal;
		Vector3f velocity_collider(0,0,0);
		/*for(int z=0; z<=1;z++)
		{
		for(int y=0; y<=1;y++)
		{
		for(int x=0; x<=1;x++)
		{
		float weight_sdf=fabs( ( p_grid_index_f[2]-(p_grid_index_i[2]-z) )*
		( p_grid_index_f[1]-(p_grid_index_i[1]-y) )*
		( p_grid_index_f[0]-(p_grid_index_i[0]-x) ) );
		Vector3i index=p_grid_index_i+Vector3i(x,y,z);
		if(inGrid(index, grid->grid_division))
		{
		Vector3f temp_normal;
		getSDFNormal(Vector3f(index[0],index[1],index[2]),temp_normal);
		sdf_normal+=temp_normal*weight_sdf;
		sdf+=grid->grids[index[0]][index[1]][index[2]]->collision_sdf*weight_sdf;
		velocity_collider+=grid->grids[index[0]][index[1]][index[2]]->collision_velocity*weight_sdf;
		}
		}
		}
		}*/


		if(getSDFNormal_box(p_grid_index_i_new,sdf_normal))
		{
			Vector3f updated_v;
			sdf_normal.normalize();
			if( updateVelocityWithSolvingCollision( velocity_collider, particles[pit]->velocity, sdf_normal, updated_v) )
			{
				particles[pit]->velocity=updated_v;
			}
		}
	}
}

void MpmCore::update_position()
{
	for(int pit=0;pit<particles.size();pit++)
		particles[pit]->position+=particles[pit]->velocity*ctrl_params.deltaT;
}

bool MpmCore::for_each_frame(int ithFrame)
{
	// ��������λ��
	int start = m_recorder.getStartFrame();
	if(ithFrame != start)
	{
		MpmStatus status;
		bool res = m_recorder.getStatus(ithFrame-1, status);
		if (res)
		{
			res = status.copy(particles);
		}
		if (!res)
		{
			return false;
		}
	}
	
	if(ctrl_params.frame!=0)
		from_particles_to_grid();

	if(ctrl_params.frame==0)
		init_particle_volume_velocity();

	compute_grid_velocity();
	solve_grid_collision();
	compute_deformation_gradient_F();
	from_grid_to_particle();
	solve_particle_collision();

	update_position();

	ctrl_params.frame++;

	// ��ģ����������
	m_recorder.addStatus(ithFrame, MpmStatus(particles));
	return true;
}

void MpmCore::create_grid()
{
	Vector3f min(-1.7, -1.38, -1.7);
	Vector3i dimensions(201, 201, 201);
	float radius=0.017f;
	Vector3f size(radius, radius, radius);

	grid=new GridField(size, min, dimensions);

	//set sdf for box
	for(int i=-100;i<=100;i++)
		for(int j=-100;j<=100;j++)
			for(int k=-100;k<100;k++)
			{
				float dis=( 90 - max( max(abs(i), abs(j)), abs(k) ) )*radius;
				grid->grids[i+100][j+100][k+100]->collision_sdf=-dis;
			}
}

void MpmCore::create_snow_ball()
{
	Vector3f snow_ball_center(0.f, 1.f, 0.f);
	Vector3i snow_ball_dimensions(40, 40, 40);
	float pmass=0.0001;
	Vector3f init_velocity(-100.0f, -100.0f, 0);
	float radius=0.017f;

	particles.clear();

	int pid=0;
	float sphereRadius = radius * (float)snow_ball_dimensions[0] / 2.0f;
	for (int x = -snow_ball_dimensions[0]/2; x <= snow_ball_dimensions[0]/2; x++) 
	{
		for (int y = -snow_ball_dimensions[1]/2; y <= snow_ball_dimensions[1]/2; y++) 
		{
			for (int z = -snow_ball_dimensions[2]/2; z <= snow_ball_dimensions[2]/2; z++) 
			{
				// generate a jittered point
				float r1 = 0.001f + static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
				float r2 = 0.001f + static_cast <float>(rand()) / static_cast <float> (RAND_MAX);
				float r3 = 0.001f + static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
				Vector3f jitter = Vector3f(r1, r2, r3) * radius;

				Vector3f pos = snow_ball_center + Vector3f(float(x), float(y), float(z)) * radius + jitter;
				// see if pos is inside the sphere
				if ((pos - snow_ball_center).norm() < sphereRadius) 
				{
					particles.push_back(new Particle(pid,pos, init_velocity, pmass));
					pid++;
				}
			}
		}
	}
}

bool MpmCore::init(int ithFrame)
{
	clear();

	ctrl_params.setting_1();

	create_snow_ball();

	create_grid();

	m_recorder.init(ithFrame);
	m_recorder.addStatus(ithFrame, MpmStatus(particles));
	return true;
}

void MpmCore::getGridConfig( Vector3f& minPnt, Vector3f& cellSize, Vector3i& cellNum )
{
	if (!grid)
	{
		minPnt.setZero();
		cellSize.setZero();
		cellNum.setZero();
	}
	else
	{
		minPnt= grid->grid_min;
		cellSize = grid->grid_size;
		cellNum = grid->grid_division;
	}
}

const vector<Particle*>& MpmCore::getParticle()
{
	return particles;
}

MpmCore::MpmCore()
{
	grid = NULL;
}

MpmCore::~MpmCore()
{
	clear();
}

void MpmCore::clear()
{
	if (grid)
	{
		delete grid;
		grid = NULL;
	}
	for (int i = 0; i < particles.size(); ++i)
	{
		if (particles[i])
		{
			delete particles[i];
		}
	}
	particles.clear();
}

StatusRecorder& MpmCore::getRecorder()
{
	return m_recorder;
}

void control_parameters::setting_1()
{
	deltaT=5e-4f;
	frame=0;

	init_youngs_modulus=4.8e4f;
	poissons_ratio=0.2f;
	hardening=15.0f;
	critical_compression=0.019f;
	critical_stretch=0.0075f;
	frictionCoeff=1.f;

	//lame coeffients
	miu=init_youngs_modulus/(2*(1+poissons_ratio));
	lambda=(poissons_ratio*init_youngs_modulus)/((1+poissons_ratio)*(1-2*poissons_ratio));

	flip_percent=0.95f;

	gravity=Vector3f(0, -9.8f, 0);
}

GridNode::GridNode()
{
	mass=0;
	velocity_new = Vector3f(0.f,0.f,0.f);
	velocity_old = Vector3f(0.f,0.f,0.f);
	external_force = Vector3f(0.f,0.f,0.f);
	collision_velocity = Vector3f(0.f,0.f,0.f);
	collision_sdf=0;

	active=false;
}
