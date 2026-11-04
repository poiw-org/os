import {Collection, EntityConstructorOrPath, getRepository, ISubCollection, SubCollection} from 'fireorm';
import {DoorState} from "../enums/DoorState";
import Lock from "./lock";

@Collection()
export default class Door {
    id: string;
    name: string;
    authorizationLevel: number;
    state: DoorState

    @SubCollection(Lock)
    locks?: ISubCollection<Lock>;

    async assignLock(lock: Lock): Promise<void> {
        await this.locks.create(lock);
    }

    async unassignLock(lock: Lock): Promise<void> {
        await this.locks.delete(lock.id);
    }

    async setState(state: DoorState): Promise<void> {
        let repo = await Door.getRepo();
        let door = await repo.findById(this.id) as Door;
        door.state = state;

        await repo.update(door)
    }

    static async create(name: string, authorizationLevel: number) {
        let door = new Door();
        door.name = name;
        door.authorizationLevel = authorizationLevel;
        door.state = DoorState.UNKNOWN;

        let repo = await Door.getRepo();

        await repo.create(door);
    }

    static getRepo = () => getRepository(Door as EntityConstructorOrPath<any>);
}