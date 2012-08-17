#include "Map.h"
#include "Math/Vec2.h"
#include "PoissonDiskSampling.h"
#include "noise/noise.h"
#include <ctime>
#include <queue>
#include <SFML/System.hpp>

const vector<vector<Biome::Type> > Map::elevation_moisture_matrix = Map::MakeBiomeMatrix();

vector<vector<Biome::Type> > Map::MakeBiomeMatrix(){
	vector<vector<Biome::Type> > matrix;
	vector<Biome::Type> biome_vector;

	biome_vector.push_back(Biome::SubtropicalDesert);
	biome_vector.push_back(Biome::TemperateDesert);
	biome_vector.push_back(Biome::TemperateDesert);
	biome_vector.push_back(Biome::Mountain);
	matrix.push_back(biome_vector);

	biome_vector.clear();
	biome_vector.push_back(Biome::Grassland);
	biome_vector.push_back(Biome::Grassland);
	biome_vector.push_back(Biome::TemperateDesert);
	biome_vector.push_back(Biome::Mountain);
	matrix.push_back(biome_vector);

	biome_vector.clear();
	biome_vector.push_back(Biome::TropicalSeasonalForest);
	biome_vector.push_back(Biome::Grassland);
	biome_vector.push_back(Biome::Shrubland);
	biome_vector.push_back(Biome::Tundra);
	matrix.push_back(biome_vector);

	biome_vector.clear();
	biome_vector.push_back(Biome::TropicalSeasonalForest);
	biome_vector.push_back(Biome::TemperateDeciduousForest);
	biome_vector.push_back(Biome::Shrubland);
	biome_vector.push_back(Biome::Snow);
	matrix.push_back(biome_vector);

	biome_vector.clear();
	biome_vector.push_back(Biome::TropicalRainForest);
	biome_vector.push_back(Biome::TemperateDeciduousForest);
	biome_vector.push_back(Biome::Taiga);
	biome_vector.push_back(Biome::Snow);
	matrix.push_back(biome_vector);

	biome_vector.clear();
	biome_vector.push_back(Biome::TropicalRainForest);
	biome_vector.push_back(Biome::TemperateRainForest);
	biome_vector.push_back(Biome::Taiga);
	biome_vector.push_back(Biome::Snow);
	matrix.push_back(biome_vector);

	return matrix;
}

Map::Map(void) {}

Map::~Map(void) {}

Map::Map(int width, int height, int point_count, string seed) : m_centers_quadtree(AABB(Vec2(width/2,height/2),Vec2(width/2,height/2)), 1){
	map_width = width;
	map_height = height;

	m_seed = seed != "" ? seed : CreateSeed(20);	

	srand(HashString(m_seed));
	//srand(884);

	z_coord = rand();
	cout << "Seed: " << m_seed << "(" << HashString(m_seed) << ")" << endl;
}

void Map::Generate() {
	sf::Clock timer;

	GeneratePolygons();

	cout << "Land distribution: ";
	timer.restart();
	GenerateLand();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	// ELEVATION
	cout << "Coast assignment: ";
	timer.restart();
	AssignOceanCoastLand();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "Corner altitude: ";
	timer.restart();
	AssignCornerElevation();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "Altitude redistribution: ";
	timer.restart();
	RedistributeElevations();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "Center altitude: ";
	timer.restart();
	AssignPolygonElevations();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	// MOISTURE
	cout << "Downslopes: ";
	timer.restart();
	CalculateDownslopes();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "River generation: ";
	timer.restart();
	GenerateRivers();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "Corner moisture: ";
	timer.restart();
	AssignCornerMoisture();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "Moisture redistribution: ";
	timer.restart();
	RedistributeMoisture();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "Center moisture: ";
	timer.restart();
	AssignPolygonMoisture();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	// BIOMES
	cout << "Biome assignment: ";
	timer.restart();
	AssignBiomes();
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;

	cout << "Populate Quadtree: ";
	timer.restart();
	for (int i = 0; i < centers.size(); i++){
		pair<Vec2,Vec2> aabb(centers[i]->GetBoundingBox());
		m_centers_quadtree.Insert2(centers[i], AABB(aabb.first, aabb.second));
	}
	cout << timer.getElapsedTime().asMicroseconds() / 1000.0 << " ms." << endl;
}

void Map::GeneratePolygons() {
	sf::Clock timer;
	GeneratePoints();
	cout << "Point placement: " << timer.getElapsedTime().asMicroseconds() / 1000.0 << "ms." << endl;
	timer.restart();
	Triangulate(points);
	cout << "Triangulation: " << timer.getElapsedTime().asMicroseconds() / 1000.0 << "ms." << endl;
	timer.restart();
	FinishInfo();
	cout << "Finishing touches: " << timer.getElapsedTime().asMicroseconds() / 1000.0 << "ms." << endl;
}

void Map::GenerateLand() {
	noiseMap = new noise::module::Perlin();	

	// Establezco los bordes del mapa
	for each(corner * c in corners) {
		if(!c->IsInsideBoundingBox(map_width, map_height)){
			c->border = true;
			c->ocean = true;
			c->water = true;
		}
	}

	// Determino lo que es agua y lo que es tierra
	for each (corner * c in corners) {
		c->water = !IsIsland(c->position);
	}
}

void Map::AssignOceanCoastLand(){
	queue<center *> centers_queue;
	// Quien es agua o border
	for each (center * c in centers) {
		int adjacent_water = 0;
		for each (corner * q in c->corners) {
			if(q->border){
				c->border = true;
				c->ocean = true;
				q->water = true;
				centers_queue.push(c);
			}
			if(q->water){
				adjacent_water++;
			}
		}
		c->water = (c->ocean || adjacent_water >= c->corners.size() * 0.5);
	}

	// Quien es oceano y quien no
	while(!centers_queue.empty()){
		center * c = centers_queue.front();
		centers_queue.pop();
		for each (center * r in c->centers)	{
			if(r->water && !r->ocean){
				r->ocean = true;
				centers_queue.push(r);
			}
		}
	}

	// Costas de center
	for each (center * p in centers) {
		int num_ocean = 0;
		int num_land = 0;
		for each (center * q in p->centers) {
			num_ocean += q->ocean;
			num_land += !q->water;
		}
		p->coast = num_land > 0 && num_ocean > 0;
	}

	// Costas de corner
	for each (corner * c in corners) {
		int adj_ocean = 0;
		int adj_land = 0;
		for each (center * p in c->centers) {
			adj_ocean += (int) p->ocean;
			adj_land += (int) !p->water;
		}
		c->ocean = adj_ocean == c->centers.size();
		c->coast = adj_land > 0 && adj_ocean > 0;
		c->water = c->border || (adj_land != c->centers.size() && !c->coast);
	}
}

void Map::AssignCornerElevation(){
	queue<corner *> corner_queue;
	for each (corner * q in corners) {
		if(q->border){
			q->elevation = 0.0;
			corner_queue.push(q);
		}else{
			q->elevation = 99999;
		}
	}

	while(!corner_queue.empty()){
		corner * q = corner_queue.front();
		corner_queue.pop();

		for each (corner * s in q->corners) {
			double new_elevation = q->elevation + 0.01;
			if(!q->water && !s->water){
				new_elevation += 1;
			}
			if(new_elevation < s->elevation){
				s->elevation = new_elevation;
				corner_queue.push(s);
			}
		}
	}

	for each (corner * q in corners) {
		if(q->water){
			q->elevation = 0.0;
		}
	}
}

void Map::RedistributeElevations(){
	vector<corner *> locations = GetLandCorners();
	double SCALE_FACTOR = 1.05;

	sort(locations.begin(), locations.end(), &corner::SortByElevation);

	for(int i = 0; i < locations.size(); i++){
		double y = (double) i / (locations.size() - 1);
		double x = sqrt(SCALE_FACTOR) - sqrt(SCALE_FACTOR * (1-y));
		x = min(x, 1.0);
		locations[i]->elevation = x;
	}
}

void Map::AssignPolygonElevations(){
	for each (center * p in centers) {
		double elevation_sum = 0.0;
		for each (corner * q in p->corners) {
			elevation_sum += q->elevation;
		}
		p->elevation = elevation_sum / p->corners.size();
	}
}

void Map::CalculateDownslopes(){
	for each (corner * c in corners) {
		corner * d = c;
		for each (corner * q in c->corners) {
			if(q->elevation < d->elevation){
				d = q;
			}
		}
		c->downslope = d;
	}
}

void Map::GenerateRivers(){
	//int num_rios = (map_height + map_width) / 4;
	int num_rios = centers.size() / 3;
	for(int i = 0; i < num_rios; i++){
		corner *q = corners[rand()%corners.size()];
		if( q->ocean || q->elevation < 0.3 || q->elevation > 0.9 ) continue;

		while(!q->coast){
			if(q == q->downslope){
				break;
			}
			edge * e = q->GetEdgeWith(q->downslope);
			e->river_volume += 1;
			q->river_volume += 1;
			q->downslope->river_volume += 1;
			q = q->downslope;
		}
	}
}

void Map::AssignCornerMoisture(){
	queue<corner *> corner_queue;
	// Agua dulce
	for each (corner * c in corners) {
		if((c->water || c->river_volume > 0) && !c->ocean){
			c->moisture = c->river_volume > 0 ? min(3.0, (0.2 * c->river_volume)) : 1.0;
			corner_queue.push(c);
		}else{
			c->moisture = 0.0;
		}
	}

	while(!corner_queue.empty()){
		corner * c = corner_queue.front();
		corner_queue.pop();

		for each (corner * r in c->corners)	{
			double new_moisture = c->moisture * 0.9;
			if( new_moisture > r->moisture ){
				r->moisture = new_moisture;
				corner_queue.push(r);
			}
		}
	}

	// Agua salada
	for each (corner * r in corners) {
		if(r->ocean){
			r->moisture = 1.0;
			corner_queue.push(r);
		}
	}
	while(!corner_queue.empty()){
		corner * c = corner_queue.front();
		corner_queue.pop();

		for each (corner * r in c->corners)	{
			double new_moisture = c->moisture * 0.3;
			if( new_moisture > r->moisture ){
				r->moisture = new_moisture;
				corner_queue.push(r);
			}
		}
	}

}

void Map::RedistributeMoisture(){
	vector<corner *> locations = GetLandCorners();

	sort(locations.begin(), locations.end(), &corner::SortByMoisture);

	for(int i = 0; i < locations.size(); i++){
		locations[i]->moisture = (double) i / (locations.size() - 1);
	}
}

void Map::AssignPolygonMoisture(){
	for each (center * p in centers) {
		double new_moisture = 0.0;
		for each (corner * q in p->corners){
			if(q->moisture > 1.0) q->moisture = 1.0;
			new_moisture += q->moisture;
		}
		p->moisture = new_moisture / p->corners.size();
	}
}

void Map::AssignBiomes(){

	for each (center * c in centers) {
		if(c->ocean){
			c->biome = Biome::Ocean;
		}else if(c->water){
			c->biome = Biome::Lake;
		}else if(c->coast && c->moisture < 0.6){
			c->biome = Biome::Beach;
		}else{
			int elevation_index = 0;
			if(c->elevation > 0.85){
				elevation_index = 3;
			}else if(c->elevation > 0.6){
				elevation_index = 2;
			}else if(elevation_index > 0.3){
				elevation_index = 1;
			}else{
				elevation_index = 0;
			}

			int moisture_index = min((int) floor(c->moisture * 6), 5);
			c->biome = elevation_moisture_matrix[moisture_index][elevation_index];
		}
	}
}

void Map::FinishInfo(){
	center::PVIter center_iter, centers_end = centers.end();
	for(center_iter = centers.begin(); center_iter != centers_end; center_iter++){
		OrderPoints((*center_iter)->corners);
	}

	for each (center * c  in centers) {
		for each (edge * e in c->edges) {
			center *aux_center = e->GetOpositeCenter(c);
			if(aux_center != NULL)
				c->centers.push_back(aux_center);
		}
	}
	for each (corner * c  in corners) {
		for each (edge * e in c->edges) {
			corner * aux_corner = e->GetOpositeCorner(c);
			if(aux_corner != NULL)
				c->corners.push_back(aux_corner);
		}
	}
}

void Map::OrderPoints( vector<corner *> & corners ) {
	vector<corner *> result;
	if (corners.size() >= 4) {
		corner * leftMostPoint = corners[0];
		corner::PVIter corner_iter, corners_end = corners.end();
		for (corner_iter = corners.begin() + 1; corner_iter != corners.end(); corner_iter++) {
			if ((*corner_iter)->position.x < leftMostPoint->position.x 
				|| ((*corner_iter)->position.x == leftMostPoint->position.x && (*corner_iter)->position.y < leftMostPoint->position.y)) {
					leftMostPoint = *corner_iter;
			}
		}
		corner * current = leftMostPoint;
		int leftMostInt;

		do {
			result.insert(result.begin(), current);
			if (current == corners[0])
				leftMostInt = 1;
			else
				leftMostInt = 0;
			for (unsigned int i = leftMostInt + 1; i < corners.size(); i++) {
				Vec2 v1(current->position, corners[leftMostInt]->position);
				Vec2 v2(current->position, corners[i]->position);
				if (v1.CrossProduct(v2) > 0) {
					leftMostInt = i;
				}
			}
			current = corners[leftMostInt];
		} while (current != result.back());
		corners = result;
	}
}

vector<corner *> Map::GetLandCorners(){
	vector<corner *> land_corners;
	for each (corner * c in corners)
		if(!c->water)
			land_corners.push_back(c);
	return land_corners;
}

vector<corner *> Map::GetLakeCorners(){
	vector<corner *> lake_corners;
	for each (corner * c in corners)
		if(c->water && !c->ocean)
			lake_corners.push_back(c);
	return lake_corners;
}

bool Map::IsIsland(Vec2 position){

	double water_threshold = 0.075;

	if(position.x < map_width * water_threshold || position.y < map_height * water_threshold
		|| position.x > map_width * (1 - water_threshold) || position.y > map_height * (1 - water_threshold))
		return false;

	Vec2 center_pos = Vec2(map_width / 2.0, map_height / 2.0);

	position -= center_pos;
	double x_coord = (position.x / map_width) * 4;
	double y_coord = (position.y / map_height) * 4;
	double noise_val = noiseMap->GetValue(x_coord, y_coord, z_coord);

	position /= min(map_width, map_height);
	double radius = position.Length();

	double factor = radius - 0.5;
	/*	if(radius > 0.3)
	factor = -1 / log(radius - 0.3) / 10;
	else
	factor = radius - 0.3;
	*/
	return noise_val >= 0.3*radius + factor;
}

void Map::Triangulate(vector<del::vertex> puntos){
	int corner_index = 0, center_index = 0, edge_index = 0;
	corners.clear();
	centers.clear();
	edges.clear();
	pos_cen_map.clear();

	del::vertexSet v (puntos.begin(), puntos.end());
	del::triangleSet tris;
	del::edgeSet edg;
	del::Delaunay dela;

	dela.Triangulate(v, tris);

	for each (del::triangle t in tris) {		
		Vec2 pos_center_0( t.GetVertex(0)->GetX(), t.GetVertex(0)->GetY());
		Vec2 pos_center_1( t.GetVertex(1)->GetX(), t.GetVertex(1)->GetY());
		Vec2 pos_center_2( t.GetVertex(2)->GetX(), t.GetVertex(2)->GetY());

		center * c1 = GetCenter(pos_center_0);
		if(c1 == NULL){
			c1 = new center(center_index++, pos_center_0);
			centers.push_back(c1);
			AddCenter(c1);
		}
		center * c2 = GetCenter(pos_center_1);
		if(c2 == NULL){
			c2 = new center(center_index++, pos_center_1);
			centers.push_back(c2);
			AddCenter(c2);
		}
		center * c3 = GetCenter(pos_center_2);
		if(c3 == NULL){
			c3 = new center(center_index++, pos_center_2);
			centers.push_back(c3);
			AddCenter(c3);
		}

		corner * c = new corner(corner_index++, Vec2());
		corners.push_back(c);
		c->centers.push_back(c1);
		c->centers.push_back(c2);
		c->centers.push_back(c3);
		c1->corners.push_back(c);
		c2->corners.push_back(c);
		c3->corners.push_back(c);
		c->position = c->CalculateCircumcenter();

		edge * e12 = c1->GetEdgeWith(c2);
		if(e12 == NULL){			
			e12 = new edge(edge_index++, c1, c2, NULL, NULL);
			e12->v0 = c;
			edges.push_back(e12);
			c1->edges.push_back(e12);
			c2->edges.push_back(e12);
		}else{
			e12->v1 = c;
		}
		c->edges.push_back(e12);

		edge * e23 = c2->GetEdgeWith(c3);
		if(e23 == NULL){			
			e23 = new edge(edge_index++, c2, c3, NULL, NULL);
			e23->v0 = c;
			edges.push_back(e23);
			c2->edges.push_back(e23);
			c3->edges.push_back(e23);
		}else{
			e23->v1 = c;
		}
		c->edges.push_back(e23);

		edge * e31 = c3->GetEdgeWith(c1);
		if(e31 == NULL){			
			e31 = new edge(edge_index++, c3, c1, NULL, NULL);
			e31->v0 = c;
			edges.push_back(e31);
			c3->edges.push_back(e31);
			c1->edges.push_back(e31);
		}else{
			e31->v1 = c;
		}
		c->edges.push_back(e31);
	}

}

void Map::AddCenter(center * c){
	map<double, map<double, center *> >::const_iterator it = pos_cen_map.find(c->position.x);
	if(it != pos_cen_map.end()){
		pos_cen_map[(int) c->position.x][c->position.y] = c;
	}else{
		pos_cen_map[(int) c->position.x] = map<double, center*>();
		pos_cen_map[(int) c->position.x][c->position.y] = c;
	}
}

center * Map::GetCenter(Vec2 position){
	map<double, map<double, center *> >::const_iterator it = pos_cen_map.find(position.x);
	if(it != pos_cen_map.end()){
		map<double, center *>::const_iterator it2 = it->second.find(position.y);
		if(it2 != it->second.end()){
			return it2->second;
		}
	}

	return NULL;
}

void Map::GeneratePoints(){
	PoissonDiskSampling pds(800, 600, 10, 10);
	vector<pair<double,double> > new_points = pds.Generate();
	cout << "Generating " << new_points.size() << " points..." << endl;
	for each (pair<double,double> p in new_points) {
		points.push_back(del::vertex((int) p.first, (int) p.second));
	}
	points.push_back(del::vertex(- map_width	,- map_height));
	points.push_back(del::vertex(2 * map_width	,- map_height));
	points.push_back(del::vertex(2 * map_width	,2 * map_height));
	points.push_back(del::vertex(- map_width	,2 * map_height));
}

void Map::LloydRelaxation(){
	vector<del::vertex> new_points;
	for each (center * p in centers) {
		if(!p->IsInsideBoundingBox(map_width, map_height)){
			new_points.push_back(del::vertex((int)p->position.x, (int) p->position.y));
			continue;
		}
		Vec2 center_centroid;		
		for each (corner * q in p->corners)	{
			if(q->IsInsideBoundingBox(map_width, map_height)){
				center_centroid += q->position;
			}else{
				Vec2 corner_pos = q->position;
				if(corner_pos.x < 0){
					corner_pos.x = 0;
				}else if(corner_pos.x >= map_width){
					corner_pos.x = map_width;
				}
				if(corner_pos.y < 0){
					corner_pos.y = 0;
				}else if(corner_pos.y >= map_height){
					corner_pos.y = map_height;
				}
				center_centroid += corner_pos;
			}
		}
		center_centroid /= p->corners.size();
		new_points.push_back(del::vertex((int)center_centroid.x, (int)center_centroid.y));
	}
	Triangulate(new_points);
}

vector<center *> Map::GetCenters(){
	return centers;
}

vector<corner *> Map::GetCorners(){
	return corners;
}

vector<edge *> Map::GetEdges(){
	return edges;
}

unsigned int Map::HashString(string seed){
	unsigned int hash = 0;
	for(int i = 0; i < seed.length(); i++) {
		hash += ((int) seed[i]) * pow(2, i);
	}
	return hash % UINT_MAX;
}

string Map::CreateSeed(int length){
	srand(time(NULL));
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	string seed;
	for (int i = 0; i < length; ++i) {
		seed.push_back(alphanum[rand() % (sizeof(alphanum) - 1)]);
	}
	return seed;
}

center * Map::GetCenterAt(Vec2 p_pos){
	center * r_center = NULL;
	sf::Clock timer;
	vector<center *> l_aux_centers(m_centers_quadtree.QueryRange(p_pos));
	int leaf_found = timer.getElapsedTime().asMicroseconds();

	double l_min_dist = Vec2(l_aux_centers[0]->position, p_pos).Length();
	r_center = l_aux_centers[0];
	for(int i = 1; i < l_aux_centers.size(); i++){
		double l_new_dist = Vec2(l_aux_centers[i]->position, p_pos).Length();
		if(l_new_dist < l_min_dist){
			l_min_dist = l_new_dist;
			r_center = l_aux_centers[i];
		}
	}


	/*
	for(int i = 0; i < l_aux_centers.size(); i++){
		if(l_aux_centers[i]->Contains(p_pos)){
			r_center = l_aux_centers[i];
			break;
		}			
	}*/

	/*
	int l_centers_size = l_aux_centers.size();
	
	if ( l_centers_size > 0 ){
		r_center = l_aux_centers.front();

		while(!r_center->Contains(p_pos)){
			double l_min_distance = Vec2(r_center->position, p_pos).Length();

			center * l_new_candidate = r_center;
			center::PVIter iter;
			for(iter = r_center->centers.begin(); iter != r_center->centers.end(); iter++){
				double l_new_dist = Vec2((*iter)->position, p_pos).Length();
				if(l_new_dist < l_min_distance){
					l_min_distance = l_new_dist;
					l_new_candidate = *iter;
				}
			}
			r_center = l_new_candidate;
		}
	}*/
	
	cout << "Found in: " << timer.getElapsedTime().asMicroseconds() / 1000.0 << "(" << leaf_found / 1000.0 << ") " << l_aux_centers.size() << endl;
	if(r_center != NULL)
		cout << "FOUND!" << endl;	
	return r_center;
}