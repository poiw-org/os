import {Collection, EntityConstructorOrPath, getRepository, ISubCollection, SubCollection} from 'fireorm';
import Key from "./key";
import {AuthenticationProvider} from "../enums/AuthenticationProvider";
import {UserType} from "../enums/UserType";

@Collection()
export default class User {
    id: string;
    name: string;
    email: string;
    authenticationProvider: AuthenticationProvider;
    type: UserType;
    isBlocked: boolean;
    authorizationLevel: number;

    static async create(email: string, authenticationProvider: AuthenticationProvider, type: UserType): Promise<User> {
            let user = new User();
            user.email = email;
            user.authenticationProvider = authenticationProvider;
            user.type = type;

            let repo = await User.getRepo();
            user = await repo.create(user);

            return user;
    }

    async delete(): Promise<void> {
        let repo = await User.getRepo();
        await repo.delete(this.id);
    }

    async assignKey(key: Key): Promise<void>{
        await this.keys.create(key);
    }

    async blockKey({id}): Promise<void> {
        let key = await this.keys.findById(id) as Key;
        key.isBlocked = true;

        await this.keys.update(key);
    }

    async unblockKey({id}): Promise<void> {
        let key = await this.keys.findById(id) as Key;
        key.isBlocked = false;

        await this.keys.update(key);
    }

    async block(){
        let repo = User.getRepo();
        let user = await repo.findById(this.id) as User;
        user.isBlocked = true;

        await repo.update(user);
    }

    async unblock(){
        let repo = User.getRepo();
        let user = await repo.findById(this.id) as User;
        user.isBlocked = false;

        await repo.update(user);
    }

    async setAuthorizationLevel(authorizationLevel: number): Promise<void> {
        let repo = User.getRepo();
        let user = await repo.findById(this.id) as User;
        user.authorizationLevel = authorizationLevel;

        await repo.update(user);
    }

    async toJSON(): Promise<any> {
        (await User.getRepo().findById(this.id)).toJSON();
    }

    static getRepo = () => getRepository(User as EntityConstructorOrPath<any>);

    @SubCollection(Key)
    keys?: ISubCollection<Key>;

}